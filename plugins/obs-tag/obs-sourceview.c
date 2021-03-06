#include <obs-module.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/darray.h>
#include <util/dstr.h>
#include "tft.h"

#define do_log(level, format, ...) \
	blog(level, "[sourceview: '%s'] " format, \
			obs_source_get_name(ss->source), ##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)

#define S_TR_SPEED                     "transition_speed"
#define S_CUSTOM_SIZE                  "use_custom_size"
#define S_SLIDE_TIME                   "slide_time"
#define S_TRANSITION                   "transition"
#define S_RANDOMIZE                    "randomize"
#define S_LOOP                         "loop"
#define S_HIDE                         "hide"
#define S_FILES                        "files"
#define S_BEHAVIOR                     "playback_behavior"
#define S_BEHAVIOR_STOP_RESTART        "stop_restart"
#define S_BEHAVIOR_PAUSE_UNPAUSE       "pause_unpause"
#define S_BEHAVIOR_ALWAYS_PLAY         "always_play"
#define S_MODE                         "slide_mode"
#define S_MODE_AUTO                    "mode_auto"
#define S_MODE_MANUAL                  "mode_manual"

#define TR_CUT                         "cut"
#define TR_FADE                        "fade"
#define TR_SWIPE                       "swipe"
#define TR_SLIDE                       "slide"

#define T_(text) obs_module_text("SourceView." text)
#define T_TR_SPEED                     T_("TransitionSpeed")
#define T_CUSTOM_SIZE                  T_("CustomSize")
#define T_CUSTOM_SIZE_AUTO             T_("CustomSize.Auto")
#define T_SLIDE_TIME                   T_("SlideTime")
#define T_TRANSITION                   T_("Transition")
#define T_RANDOMIZE                    T_("Randomize")
#define T_LOOP                         T_("Loop")
#define T_HIDE                         T_("HideWhenDone")
#define T_FILES                        T_("Files")
#define T_BEHAVIOR                     T_("PlaybackBehavior")
#define T_BEHAVIOR_STOP_RESTART        T_("PlaybackBehavior.StopRestart")
#define T_BEHAVIOR_PAUSE_UNPAUSE       T_("PlaybackBehavior.PauseUnpause")
#define T_BEHAVIOR_ALWAYS_PLAY         T_("PlaybackBehavior.AlwaysPlay")
#define T_MODE                         T_("SlideMode")
#define T_MODE_AUTO                    T_("SlideMode.Auto")
#define T_MODE_MANUAL                  T_("SlideMode.Manual")

#define T_TR_(text) obs_module_text("SourceView.Transition." text)
#define T_TR_CUT                       T_TR_("Cut")
#define T_TR_FADE                      T_TR_("Fade")
#define T_TR_SWIPE                     T_TR_("Swipe")
#define T_TR_SLIDE                     T_TR_("Slide")

/* ------------------------------------------------------------------------- */

#define MOD(a,b) ((((a)%(b))+(b))%(b))
#define MAX_LOADED 15 /* needs to be an odd number */

struct image_file_data {
	char *path;
	obs_source_t *source;
};

enum behavior {
	BEHAVIOR_STOP_RESTART,
	BEHAVIOR_PAUSE_UNPAUSE,
	BEHAVIOR_ALWAYS_PLAY,
};

struct sourceview {
	obs_source_t *source;
    tft_buffer_t *tftBuffer;
    obs_source_t *current_file_source;

	bool randomize;
	bool loop;
	bool restart_on_activate;
	bool pause_on_deactivate;
	bool manual;
	bool hide;
	bool use_cut;
	bool paused;
	bool stop;
	float slide_time;
	uint32_t tr_speed;
	const char *tr_name;
	obs_source_t *transition;

	float elapsed;
	int cur_item;

	uint32_t cx;
	uint32_t cy;

	pthread_mutex_t mutex;
	DARRAY(struct image_file_data) files;
	DARRAY(char*) paths;

	enum behavior behavior;

	obs_hotkey_id play_pause_hotkey;
	obs_hotkey_id restart_hotkey;
	obs_hotkey_id stop_hotkey;
	obs_hotkey_id next_hotkey;
	obs_hotkey_id prev_hotkey;
};

static obs_source_t *get_transition(struct sourceview *ss)
{
	obs_source_t *tr;

	pthread_mutex_lock(&ss->mutex);
	tr = ss->transition;
	obs_source_addref(tr);
	pthread_mutex_unlock(&ss->mutex);

	return tr;
}

static obs_source_t *get_source(struct darray *array, const char *path)
{
	DARRAY(struct image_file_data) files;
	obs_source_t *source = NULL;

	files.da = *array;

	for (size_t i = 0; i < files.num; i++) {
		const char *cur_path = files.array[i].path;

		if (strcmp(path, cur_path) == 0) {
			source = files.array[i].source;
			obs_source_addref(source);
			break;
		}
	}

	return source;
}

static obs_source_t *create_source_from_file(const char *file)
{
	obs_data_t *settings = obs_data_create();
	obs_source_t *source;

	obs_data_set_string(settings, "file", file);
	obs_data_set_bool(settings, "unload", false);
	source = obs_source_create_private("image_source", NULL, settings);

	obs_data_release(settings);
	return source;
}

static void free_files(struct darray *array)
{
	DARRAY(struct image_file_data) files;
	files.da = *array;

	for (size_t i = 0; i < files.num; i++) {
		bfree(files.array[i].path);
		obs_source_release(files.array[i].source);
	}

	da_free(files);
}

static inline size_t random_file(struct sourceview *ss)
{
	return (size_t)rand() % ss->paths.num;
}

static void free_paths(struct darray *array)
{
	DARRAY(char*) paths;
	paths.da = *array;

	for (size_t i = 0; i < paths.num; i++)
		bfree(paths.array[i]);

	da_free(paths);
}

/* ------------------------------------------------------------------------- */

static const char *ss_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("SourceView");
}

static void add_file(struct sourceview *ss, struct darray *array,
		const char *path, uint32_t *cx, uint32_t *cy, bool next)
{
	DARRAY(struct image_file_data) new_files;
	struct image_file_data data;
	obs_source_t *new_source;

	new_files.da = *array;

	pthread_mutex_lock(&ss->mutex);
	new_source = get_source(&ss->files.da, path);
	pthread_mutex_unlock(&ss->mutex);

	if (!new_source)
		new_source = get_source(&new_files.da, path);
	if (!new_source)
		new_source = create_source_from_file(path);

	if (new_source) {
		uint32_t new_cx = obs_source_get_width(new_source);
		uint32_t new_cy = obs_source_get_height(new_source);

		data.path = bstrdup(path);
		data.source = new_source;

		if (next)
			da_push_back(new_files, &data);
		else
			da_insert(new_files, 0, &data);

		if (new_cx > *cx) *cx = new_cx;
		if (new_cy > *cy) *cy = new_cy;
	}

	*array = new_files.da;
}

static void clear_buffer(struct sourceview *ss, bool next)
{
	if (ss->paths.num <= MAX_LOADED || !ss->paths.num || !ss->files.num)
		return;

	if (next) {
		bfree(ss->files.array[0].path);
		obs_source_release(ss->files.array[0].source);
		da_erase(ss->files, 0);
	} else {
		bfree(ss->files.array[ss->files.num - 1].path);
		obs_source_release(ss->files.array[ss->files.num - 1].source);
		da_pop_back(ss->files);
	}

	size_t index = 0;

	if (ss->randomize)
		index = random_file(ss);
	else if (!ss->randomize && next)
		index = MOD((ss->cur_item + ((MAX_LOADED / 2) + 1)),
				ss->paths.num);
	else if (!ss->randomize && !next)
		index = MOD((ss->cur_item - ((MAX_LOADED / 2) + 1)),
				ss->paths.num);

	uint32_t cx, cy;
	add_file(ss, &ss->files.da, ss->paths.array[index], &cx, &cy,
			next);
}

static void add_path(struct darray *array, const char *path)
{
	DARRAY(char*) new_paths;
	new_paths.da = *array;

	const char *new_path = bstrdup(path);
	da_push_back(new_paths, &new_path);

	*array = new_paths.da;
}

static bool valid_extension(const char *ext)
{
	if (!ext)
		return false;
	return astrcmpi(ext, ".bmp") == 0 ||
	       astrcmpi(ext, ".tga") == 0 ||
	       astrcmpi(ext, ".png") == 0 ||
	       astrcmpi(ext, ".jpeg") == 0 ||
	       astrcmpi(ext, ".jpg") == 0 ||
	       astrcmpi(ext, ".gif") == 0;
}

static inline bool item_valid(struct sourceview *ss)
{
	return ss->files.num && ss->paths.num &&
			(size_t)ss->cur_item < ss->paths.num;
}


static long batch_getref(obs_source_t *source)
{
    const char *file;
	obs_data_t *settings = obs_source_get_settings(source);
	file = obs_data_get_string(settings, "file");
    /* find short name */
    const char * shortpath = strrchr(file,'/');
    if(shortpath == NULL) shortpath = file;
    else shortpath++; /*skip '/'*/
    
    /* skip ext name */
    char * filename = strdup(shortpath);
    char * extname = strrchr(shortpath, '.');
    if(filename == NULL) return 0;
    else {
        extname = '\0';
        extname++;
        return atol(filename);
    }
}
        
static void do_batch_active(struct sourceview *ss, obs_source_t *source)
{
    long ref;
	if (!source || !ss)
		return;

    uint32_t width = obs_source_get_width(source);
    uint32_t height = obs_source_get_height(source);
    ref = batch_getref(source);
    printf("do_batch_active(%li).\n", ref);

    /* add scene to tftbuffer */
    if (ss->tftBuffer->scene == NULL) {
        ss->tftBuffer->scene = obs_source_get_scene(ss->source);
        obs_scene_set_handle(ss->tftBuffer->scene, ss->tftBuffer);
    }

    tft_batch_set(ss->tftBuffer, ref, width, height);
}

static void do_transition(void *data, bool to_null, bool next)
{
	struct sourceview *ss = data;
	bool valid = item_valid(ss);

	if (to_null) {
		obs_transition_start(ss->transition, OBS_TRANSITION_MODE_AUTO,
				ss->tr_speed,
				NULL);
		return;
	}

	if (!valid)
		return;

	clear_buffer(ss, next);

	obs_source_t *source;

	if (next && ss->paths.num > MAX_LOADED)
		source = ss->files.array[(MAX_LOADED / 2) + 1].source;
	else if (!next && ss->paths.num > MAX_LOADED)
		source = ss->files.array[(MAX_LOADED / 2) - 1].source;
	else if (ss->paths.num <= MAX_LOADED)
		source = ss->files.array[(size_t)ss->cur_item].source;

	if (!source)
		return;

    //LEO: call tft to udpate box status in current batch
    if (ss->current_file_source != source) {
        ss->current_file_source = source;
        do_batch_active(ss, source);
    }

	if (ss->use_cut)
		obs_transition_set(ss->transition, source);

	else if (!to_null)
		obs_transition_start(ss->transition,
				OBS_TRANSITION_MODE_AUTO,
				ss->tr_speed, source);
}

static void ss_update(void *data, obs_data_t *settings)
{
	DARRAY(struct image_file_data) new_files;
	DARRAY(struct image_file_data) old_files;
	DARRAY(char*) new_paths;
	DARRAY(char*) old_paths;
	obs_source_t *new_tr = NULL;
	obs_source_t *old_tr = NULL;
	struct sourceview *ss = data;
	obs_data_array_t *array;
	const char *tr_name;
	uint32_t new_duration;
	uint32_t new_speed;
	uint32_t cx = 0;
	uint32_t cy = 0;
	uint32_t last_cx = 0;
	uint32_t last_cy = 0;
	size_t count;
	const char *behavior;
	const char *mode;


	/* ------------------------------------- */
	/* get settings data */

	da_init(new_files);
	da_init(new_paths);

	behavior = obs_data_get_string(settings, S_BEHAVIOR);

	if (astrcmpi(behavior, S_BEHAVIOR_PAUSE_UNPAUSE) == 0)
		ss->behavior = BEHAVIOR_PAUSE_UNPAUSE;
	else if (astrcmpi(behavior, S_BEHAVIOR_ALWAYS_PLAY) == 0)
		ss->behavior = BEHAVIOR_ALWAYS_PLAY;
	else /* S_BEHAVIOR_STOP_RESTART */
		ss->behavior = BEHAVIOR_STOP_RESTART;

	mode = obs_data_get_string(settings, S_MODE);

	ss->manual = (astrcmpi(mode, S_MODE_MANUAL) == 0);

	tr_name = obs_data_get_string(settings, S_TRANSITION);
	if (astrcmpi(tr_name, TR_CUT) == 0)
		tr_name = "cut_transition";
	else if (astrcmpi(tr_name, TR_SWIPE) == 0)
		tr_name = "swipe_transition";
	else if (astrcmpi(tr_name, TR_SLIDE) == 0)
		tr_name = "slide_transition";
	else
		tr_name = "fade_transition";

	ss->randomize = obs_data_get_bool(settings, S_RANDOMIZE);
	ss->loop = obs_data_get_bool(settings, S_LOOP);
	ss->hide = obs_data_get_bool(settings, S_HIDE);

	if (!ss->tr_name || strcmp(tr_name, ss->tr_name) != 0)
		new_tr = obs_source_create_private(tr_name, NULL, NULL);

	new_duration = (uint32_t)obs_data_get_int(settings, S_SLIDE_TIME);
	new_speed = (uint32_t)obs_data_get_int(settings, S_TR_SPEED);

	array = obs_data_get_array(settings, S_FILES);
	count = obs_data_array_count(array);

	/* ------------------------------------- */
	/* create new list of sources */

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *path = obs_data_get_string(item, "value");
		os_dir_t *dir = os_opendir(path);

		if (dir) {
			struct dstr dir_path = {0};
			struct os_dirent *ent;

			for (;;) {
				const char *ext;

				ent = os_readdir(dir);
				if (!ent)
					break;
				if (ent->directory)
					continue;

				ext = os_get_path_extension(ent->d_name);
				if (!valid_extension(ext))
					continue;

				dstr_copy(&dir_path, path);
				dstr_cat_ch(&dir_path, '/');
				dstr_cat(&dir_path, ent->d_name);
				add_path(&new_paths.da, dir_path.array);
			}

			dstr_free(&dir_path);
			os_closedir(dir);
		} else {
			add_path(&new_paths.da, path);
		}

		obs_data_release(item);
	}

	/* ------------------------------------- */
	/* update settings data */

	pthread_mutex_lock(&ss->mutex);

	old_files.da = ss->files.da;
	ss->files.da = new_files.da;
	old_paths.da = ss->paths.da;
	ss->paths.da = new_paths.da;
	if (new_tr) {
		old_tr = ss->transition;
		ss->transition = new_tr;
	}

	if (new_duration < 50)
		new_duration = 50;
	if (new_speed > (new_duration - 50))
		new_speed = new_duration - 50;

	ss->tr_speed = new_speed;
	ss->tr_name = tr_name;
	ss->slide_time = (float)new_duration / 1000.0f;

	pthread_mutex_unlock(&ss->mutex);

	if (ss->paths.num > MAX_LOADED && !ss->randomize) {
		for (int i = -(MAX_LOADED / 2); i <= (MAX_LOADED / 2); i++) {
			size_t index = MOD(i, ss->paths.num);
			add_file(ss, &ss->files.da, ss->paths.array[index],
					&cx, &cy, true);
		}
	} else if (ss->paths.num > MAX_LOADED && ss->randomize)  {
		for (size_t i = 0; i < MAX_LOADED; i++) {
			size_t index = random_file(ss);
			add_file(ss, &ss->files.da, ss->paths.array[index],
					&cx, &cy, true);
		}
	} else if (ss->paths.num <= MAX_LOADED) {
		for (size_t i = 0; i < ss->paths.num; i++)
			add_file(ss, &ss->files.da, ss->paths.array[i],
					&cx, &cy, true);
	}

	/* ------------------------------------- */
	/* clean up and restart transition */

	if (old_tr)
		obs_source_release(old_tr);
	free_files(&old_files.da);
	free_paths(&old_paths.da);

	/* ------------------------- */

	const char *res_str = obs_data_get_string(settings, S_CUSTOM_SIZE);
	bool aspect_only = false, use_auto = true;
	int cx_in = 0, cy_in = 0;

	if (strcmp(res_str, T_CUSTOM_SIZE_AUTO) != 0) {
		int ret = sscanf(res_str, "%dx%d", &cx_in, &cy_in);
		if (ret == 2) {
			aspect_only = false;
			use_auto = false;
		} else {
			ret = sscanf(res_str, "%d:%d", &cx_in, &cy_in);
			if (ret == 2) {
				aspect_only = true;
				use_auto = false;
			}
		}
	}

	if (!use_auto) {
		double cx_f = (double)cx;
		double cy_f = (double)cy;

		double old_aspect = cx_f / cy_f;
		double new_aspect = (double)cx_in / (double)cy_in;

		if (aspect_only) {
			if (fabs(old_aspect - new_aspect) > EPSILON) {
				if (new_aspect > old_aspect)
					cx = (uint32_t)(cy_f * new_aspect);
				else
					cy = (uint32_t)(cx_f / new_aspect);
			}
		} else {
			cx = (uint32_t)cx_in;
			cy = (uint32_t)cy_in;
		}
	}

	/* ------------------------- */

	obs_data_t *priv_settings = obs_source_get_private_settings(ss->source);
	last_cx = (uint32_t)obs_data_get_int(priv_settings, "last_cx");
	last_cy = (uint32_t)obs_data_get_int(priv_settings, "last_cy");

	if (ss->randomize && last_cx > 0 && last_cy > 0) {
		cx = last_cx;
		cy = last_cy;
	}

	obs_data_set_int(priv_settings, "last_cx", cx);
	obs_data_set_int(priv_settings, "last_cy", cy);
	obs_data_release(priv_settings);

	ss->cx = cx;
	ss->cy = cy;
	ss->elapsed = 0.0f;
	ss->cur_item = 0;
	obs_transition_set_size(ss->transition, cx, cy);
	obs_transition_set_alignment(ss->transition, OBS_ALIGN_CENTER);
	obs_transition_set_scale_type(ss->transition,
			OBS_TRANSITION_SCALE_ASPECT);

	if (new_tr)
		obs_source_add_active_child(ss->source, new_tr);
	if (ss->files.num)
		do_transition(ss, false, true);

	obs_data_array_release(array);
}

static void ss_play_pause(void *data)
{
	struct sourceview *ss = data;

	ss->paused = !ss->paused;
	ss->manual = ss->paused;
}

static void ss_restart(void *data)
{
	struct sourceview *ss = data;

	ss->stop = false;
	ss->use_cut = true;
	ss->restart_on_activate = false;

	obs_data_t *settings = obs_source_get_settings(ss->source);
	ss_update(ss, settings);
	obs_data_release(settings);

	ss->use_cut = false;
}

static void ss_stop(void *data)
{
	struct sourceview *ss = data;

	ss->elapsed = 0.0f;
	ss->cur_item = 0;

	do_transition(ss, true, true);
	ss->stop = true;
	ss->paused = false;
}

static void ss_next_slide(void *data)
{
	struct sourceview *ss = data;

	if (!ss->paths.num)
		return;

	if (++ss->cur_item >= (int)ss->paths.num)
		ss->cur_item = 0;

	do_transition(ss, false, true);
}

static void ss_previous_slide(void *data)
{
	struct sourceview *ss = data;

	if (!ss->paths.num)
		return;

	if (--ss->cur_item < 0)
		ss->cur_item = (int)(ss->paths.num - 1);

	do_transition(ss, false, false);
}

static void play_pause_hotkey(void *data, obs_hotkey_id id,
		obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct sourceview *ss = data;

	if (pressed && obs_source_active(ss->source))
		ss_play_pause(ss);
}

static void restart_hotkey(void *data, obs_hotkey_id id,
		obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct sourceview *ss = data;

	if (pressed && obs_source_active(ss->source))
		ss_restart(ss);
}

static void stop_hotkey(void *data, obs_hotkey_id id,
		obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct sourceview *ss = data;

	if (pressed && obs_source_active(ss->source))
		ss_stop(ss);
}

static void next_slide_hotkey(void *data, obs_hotkey_id id,
		obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct sourceview *ss = data;

	if (!ss->manual)
		return;

	if (pressed && obs_source_active(ss->source)) {
        /* LEO: update tft */
        tft_batch_update_from_obs(ss->tftBuffer);
		ss_next_slide(ss);
    }
}

static void previous_slide_hotkey(void *data, obs_hotkey_id id,
		obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct sourceview *ss = data;

	if (!ss->manual)
		return;

	if (pressed && obs_source_active(ss->source)) {
        /* LEO: update tft */
        tft_batch_update_from_obs(ss->tftBuffer);
		ss_previous_slide(ss);
    }
}

static void ss_destroy(void *data)
{
	struct sourceview *ss = data;

    /* LEO: release buffer and exit */
    tft_exit(ss->tftBuffer);

	obs_source_release(ss->transition);
	free_files(&ss->files.da);
	free_paths(&ss->paths.da);
	pthread_mutex_destroy(&ss->mutex);
	bfree(ss);
}

static void *ss_create(obs_data_t *settings, obs_source_t *source)
{
	struct sourceview *ss = bzalloc(sizeof(*ss));

/*
    obs_source_t *scenesrc;
    obs_scene_t *scene;
    scenesrc = obs_get_source_by_name("leo");
    scene = obs_scene_from_source(scenesrc);
*/
	ss->source = source;

    printf("ss_create: scene = %p\n", obs_source_get_scene(ss->source));

	ss->manual = false;
	ss->paused = false;
	ss->stop = false;

	ss->play_pause_hotkey = obs_hotkey_register_source(source,
			"SourceView.PlayPause",
			obs_module_text("SourceView.PlayPause"),
			play_pause_hotkey, ss);

	ss->restart_hotkey = obs_hotkey_register_source(source,
			"SourceView.Restart",
			obs_module_text("SourceView.Restart"),
			restart_hotkey, ss);

	ss->stop_hotkey = obs_hotkey_register_source(source,
			"SourceView.Stop",
			obs_module_text("SourceView.Stop"),
			stop_hotkey, ss);

	ss->prev_hotkey = obs_hotkey_register_source(source,
			"SourceView.NextSlide",
			obs_module_text("SourceView.NextSlide"),
			next_slide_hotkey, ss);

	ss->prev_hotkey = obs_hotkey_register_source(source,
			"SourceView.PreviousSlide",
			obs_module_text("SourceView.PreviousSlide"),
			previous_slide_hotkey, ss);


    ss->tftBuffer = tft_buffer_load(NULL, "/home/leo/datas/arch/main.json");
    tft_buffer_print(ss->tftBuffer);
    assert(ss->tftBuffer != NULL);

    ss->current_file_source = NULL;

	pthread_mutex_init_value(&ss->mutex);
	if (pthread_mutex_init(&ss->mutex, NULL) != 0)
		goto error;

	obs_source_update(source, NULL);

	UNUSED_PARAMETER(settings);
	return ss;

error:
	ss_destroy(ss);
	return NULL;
}

static void ss_video_render(void *data, gs_effect_t *effect)
{
	struct sourceview *ss = data;
	obs_source_t *transition = get_transition(ss);

	if (transition) {
		obs_source_video_render(transition);
		obs_source_release(transition);
	}

	UNUSED_PARAMETER(effect);
}

static void ss_video_tick(void *data, float seconds)
{
	struct sourceview *ss = data;

    /*LEO: chage tft update position from hotkey/enum to video tick */
    tft_batch_active(ss->tftBuffer);

	if (!ss->transition || !ss->slide_time)
		return;

	if (ss->restart_on_activate && ss->use_cut) {
		ss_restart(ss);
		return;
	}

	if (ss->pause_on_deactivate || ss->manual || ss->stop || ss->paused)
		return;

	/* ----------------------------------------------------- */
	/* fade to transparency when the file list becomes empty */
	if (!ss->paths.num) {
		obs_source_t* active_transition_source =
			obs_transition_get_active_source(ss->transition);

		if (active_transition_source) {
			obs_source_release(active_transition_source);
			do_transition(ss, true, true);
		}
	}

	/* ----------------------------------------------------- */
	/* do transition when slide time reached                 */
	ss->elapsed += seconds;

	if (ss->elapsed > ss->slide_time) {
		ss->elapsed -= ss->slide_time;

		if (!ss->loop && ss->cur_item == (int)ss->paths.num - 1 &&
				!ss->randomize) {
			if (ss->hide)
				do_transition(ss, true, true);
			else
				do_transition(ss, false, true);

			return;
		}

		ss_next_slide(ss);
	}

}

static inline bool ss_audio_render_(obs_source_t *transition, uint64_t *ts_out,
		struct obs_source_audio_mix *audio_output,
		uint32_t mixers, size_t channels, size_t sample_rate)
{
	struct obs_source_audio_mix child_audio;
	uint64_t source_ts;

	if (obs_source_audio_pending(transition))
		return false;

	source_ts = obs_source_get_audio_timestamp(transition);
	if (!source_ts)
		return false;

	obs_source_get_audio_mix(transition, &child_audio);
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		if ((mixers & (1 << mix)) == 0)
			continue;

		for (size_t ch = 0; ch < channels; ch++) {
			float *out = audio_output->output[mix].data[ch];
			float *in = child_audio.output[mix].data[ch];

			memcpy(out, in, AUDIO_OUTPUT_FRAMES *
					MAX_AUDIO_CHANNELS * sizeof(float));
		}
	}

	*ts_out = source_ts;

	UNUSED_PARAMETER(sample_rate);
	return true;
}

static bool ss_audio_render(void *data, uint64_t *ts_out,
		struct obs_source_audio_mix *audio_output,
		uint32_t mixers, size_t channels, size_t sample_rate)
{
	struct sourceview *ss = data;
	obs_source_t *transition = get_transition(ss);
	bool success;

	if (!transition)
		return false;

	success = ss_audio_render_(transition, ts_out, audio_output, mixers,
			channels, sample_rate);

	obs_source_release(transition);
	return success;
}

static void ss_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	struct sourceview *ss = data;

	pthread_mutex_lock(&ss->mutex);
	if (ss->transition)
		cb(ss->source, ss->transition, param);
	pthread_mutex_unlock(&ss->mutex);
}

static uint32_t ss_width(void *data)
{
	struct sourceview *ss = data;
	return ss->transition ? ss->cx : 0;
}

static uint32_t ss_height(void *data)
{
	struct sourceview *ss = data;
	return ss->transition ? ss->cy : 0;
}

static void ss_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_TRANSITION, "fade");
	obs_data_set_default_int(settings, S_SLIDE_TIME, 8000);
	obs_data_set_default_int(settings, S_TR_SPEED, 700);
	obs_data_set_default_string(settings, S_CUSTOM_SIZE, T_CUSTOM_SIZE_AUTO);
	obs_data_set_default_string(settings, S_BEHAVIOR,
			S_BEHAVIOR_ALWAYS_PLAY);
	obs_data_set_default_string(settings, S_MODE, S_MODE_AUTO);
	obs_data_set_default_bool(settings, S_LOOP, true);
}

static const char *file_filter =
	"Image files (*.bmp *.tga *.png *.jpeg *.jpg *.gif)";

static const char *aspects[] = {
	"16:9",
	"16:10",
	"4:3",
	"1:1"
};

#define NUM_ASPECTS (sizeof(aspects) / sizeof(const char *))

static obs_properties_t *ss_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	struct sourceview *ss = data;
	struct obs_video_info ovi;
	struct dstr path = {0};
	obs_property_t *p;
	int cx;
	int cy;

	/* ----------------- */

	obs_properties_set_flags(ppts, OBS_PROPERTIES_DEFER_UPDATE);

	obs_get_video_info(&ovi);
	cx = (int)ovi.base_width;
	cy = (int)ovi.base_height;

	/* ----------------- */

	p = obs_properties_add_list(ppts, S_BEHAVIOR, T_BEHAVIOR,
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_BEHAVIOR_ALWAYS_PLAY,
			S_BEHAVIOR_ALWAYS_PLAY);
	obs_property_list_add_string(p, T_BEHAVIOR_STOP_RESTART,
			S_BEHAVIOR_STOP_RESTART);
	obs_property_list_add_string(p, T_BEHAVIOR_PAUSE_UNPAUSE,
			S_BEHAVIOR_PAUSE_UNPAUSE);

	p = obs_properties_add_list(ppts, S_MODE, T_MODE,
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_MODE_AUTO, S_MODE_AUTO);
	obs_property_list_add_string(p, T_MODE_MANUAL, S_MODE_MANUAL);

	p = obs_properties_add_list(ppts, S_TRANSITION, T_TRANSITION,
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_TR_CUT, TR_CUT);
	obs_property_list_add_string(p, T_TR_FADE, TR_FADE);
	obs_property_list_add_string(p, T_TR_SWIPE, TR_SWIPE);
	obs_property_list_add_string(p, T_TR_SLIDE, TR_SLIDE);

	obs_properties_add_int(ppts, S_SLIDE_TIME, T_SLIDE_TIME,
			50, 3600000, 50);
	obs_properties_add_int(ppts, S_TR_SPEED, T_TR_SPEED,
			0, 3600000, 50);
	obs_properties_add_bool(ppts, S_LOOP, T_LOOP);
	obs_properties_add_bool(ppts, S_HIDE, T_HIDE);
	obs_properties_add_bool(ppts, S_RANDOMIZE, T_RANDOMIZE);

	p = obs_properties_add_list(ppts, S_CUSTOM_SIZE, T_CUSTOM_SIZE,
			OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p, T_CUSTOM_SIZE_AUTO, T_CUSTOM_SIZE_AUTO);

	for (size_t i = 0; i < NUM_ASPECTS; i++)
		obs_property_list_add_string(p, aspects[i], aspects[i]);

	char str[32];
	snprintf(str, 32, "%dx%d", cx, cy);
	obs_property_list_add_string(p, str, str);

	if (ss) {
		pthread_mutex_lock(&ss->mutex);
		if (ss->paths.num) {
			char **p_last_path = da_end(ss->paths);
			const char *last_path;
			const char *slash;

			last_path = p_last_path ? *p_last_path : "";

			dstr_copy(&path, last_path);
			dstr_replace(&path, "\\", "/");
			slash = strrchr(path.array, '/');
			if (slash)
				dstr_resize(&path, slash - path.array + 1);
		}
		pthread_mutex_unlock(&ss->mutex);
	}

	obs_properties_add_editable_list(ppts, S_FILES, T_FILES,
			OBS_EDITABLE_LIST_TYPE_FILES, file_filter, path.array);
	dstr_free(&path);

	return ppts;
}

static void ss_activate(void *data)
{
	struct sourceview *ss = data;

	if (ss->behavior == BEHAVIOR_STOP_RESTART) {
		ss->restart_on_activate = true;
		ss->use_cut = true;
	} else if (ss->behavior == BEHAVIOR_PAUSE_UNPAUSE) {
		ss->pause_on_deactivate = false;
	}
}

static void ss_deactivate(void *data)
{
	struct sourceview *ss = data;

	if (ss->behavior == BEHAVIOR_PAUSE_UNPAUSE)
		ss->pause_on_deactivate = true;
}

static bool ss_save(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
	struct sourceview *ss = data;

    tft_buffer_save(ss->tftBuffer, "/home/leo/datas/arch/main.json");
    return true;
}

struct obs_source_info sourceview_info = {
	.id                  = "sourceview",
	.type                = OBS_SOURCE_TYPE_INPUT,
	.output_flags        = OBS_SOURCE_VIDEO |
	                       OBS_SOURCE_CUSTOM_DRAW |
	                       OBS_SOURCE_COMPOSITE,
	.get_name            = ss_getname,
	.create              = ss_create,
	.save                = ss_save,
	.destroy             = ss_destroy,
	.update              = ss_update,
	.activate            = ss_activate,
	.deactivate          = ss_deactivate,
	.video_render        = ss_video_render,
	.video_tick          = ss_video_tick,
	.audio_render        = ss_audio_render,
	.enum_active_sources = ss_enum_sources,
	.get_width           = ss_width,
	.get_height          = ss_height,
	.get_defaults        = ss_defaults,
	.get_properties      = ss_properties
};

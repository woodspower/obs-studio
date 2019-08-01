#include <obs-module.h>
#include "tft.h"

struct obs_box {
    const char *fullname;
    const char *name;
    int seq;
    struct vec2 pos;

	uint32_t color;
	uint32_t width;
	uint32_t height;
	uint32_t synced;

	obs_source_t *src;
	obs_scene_t *scene;
};

static const char *obs_box_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Box");
}

static void obs_box_create_boxgroup(struct obs_box *box)
{
    obs_source_t *source = NULL;
    obs_sceneitem_t *boxitem = NULL;
    obs_sceneitem_t *txtitem = NULL;
    obs_sceneitem_t *seqitem = NULL;
    obs_sceneitem_t *group = NULL;
    obs_scene_t *scene = box->scene;

    struct vec2 tmpPos;
    static char tmpName[200];

    /* create title text box */
    if (txtitem == NULL) {
        obs_data_t *txtcfg = obs_data_create();
        obs_data_set_string(txtcfg, "text", box->name);
        obs_data_t *font_obj = obs_data_create();
        obs_data_set_int(font_obj, "size", 20);
        obs_data_set_obj(txtcfg, "font", font_obj);
        obs_data_set_int(txtcfg, "color1", 0xFF000000);
        obs_data_set_int(txtcfg, "color2", 0xFF000000);
        snprintf(tmpName, 199, "tit:%s", box->name);
        source = obs_source_create("text_ft2_source", tmpName, txtcfg, NULL);
        obs_data_release(txtcfg);
        txtitem = obs_scene_add(scene, source);
        obs_source_release(source);
        obs_sceneitem_select(txtitem, false);
        obs_sceneitem_set_visible(txtitem, true);
        /* only leave group unlocked */
        obs_sceneitem_set_locked(txtitem, true);
    }

    /* create seq text box */
    if (seqitem == NULL) {
        obs_data_t *txtcfg = obs_data_create();
        snprintf(tmpName, 199, "%d", box->seq);
        obs_data_set_string(txtcfg, "text", tmpName);
        obs_data_t *font_obj = obs_data_create();
        obs_data_set_int(font_obj, "size", 16);
        obs_data_set_obj(txtcfg, "font", font_obj);
        obs_data_set_int(txtcfg, "color1", 0xFF000000);
        obs_data_set_int(txtcfg, "color2", 0xFF000000);
        snprintf(tmpName, 199, "seq:%d", box->seq);
        source = obs_source_create("text_ft2_source", tmpName, txtcfg, NULL);
        obs_data_release(txtcfg);
        seqitem = obs_scene_add(scene, source);
        obs_source_release(source);
        obs_sceneitem_select(seqitem, false);
        obs_sceneitem_set_visible(seqitem, true);
        /* only leave group unlocked */
        obs_sceneitem_set_locked(seqitem, true);
    }

    /* create main box and opcity filter */
    if (boxitem == NULL) {
        source = box->src;
        /* create opcity filter */
        obs_data_t *fset = obs_data_create();
        obs_data_set_int(fset, "opacity", 20);
        obs_source_t *filter = obs_source_create("chroma_key_filter",
            "opcity filter", fset, NULL);
        obs_data_release(fset);
        if (filter == NULL)
            printf("create opcity filter failed.\n");
        else {
            obs_source_filter_add(source, filter);
            obs_source_release(filter);
        }
        /* add box into scene */
        boxitem = obs_scene_add(scene, source);
        printf("boxitem %s added: %p", box->fullname, boxitem);
        obs_source_release(source);
        obs_sceneitem_select(boxitem, false);
        obs_sceneitem_set_visible(boxitem, true);
        /* only leave group unlocked */
        obs_sceneitem_set_locked(boxitem, true);
    }

    /* create new group */
    snprintf(tmpName, 199, "group:%s", box->fullname);
    group = obs_scene_add_group(scene, tmpName);
    obs_sceneitem_set_pos(group, &box->pos);

    /* add sceneitems into group */
    obs_sceneitem_group_add_item(group, boxitem);
    obs_sceneitem_group_add_item(group, txtitem);
    obs_sceneitem_group_add_item(group, seqitem);
    tmpPos.x= 0;
    tmpPos.y= box->height - 20;
    obs_sceneitem_set_pos(seqitem, &tmpPos);
    obs_sceneitem_set_visible(group, true);

    return;
}


static void obs_box_update(void *data, obs_data_t *settings)
{
	struct obs_box *context = data;

	context->color = (uint32_t)obs_data_get_int(settings, "color");
	context->width = (uint32_t)obs_data_get_int(settings, "width"); 
    context->height = (uint32_t)obs_data_get_int(settings, "height");
	context->synced = (uint32_t)obs_data_get_int(settings, "synced");

	context->fullname = obs_data_get_string(settings, "fullname");
	context->name = obs_data_get_string(settings, "name");
	context->seq = obs_data_get_int(settings, "seq");
	context->pos.x = (float)obs_data_get_double(settings, "x");
	context->pos.y = (float)obs_data_get_double(settings, "y");

    context->scene = (obs_scene_t*)(long)obs_data_get_double(settings, "scene");

    if(context->scene != NULL) {
        printf("obs_box_create: %s\n", context->fullname);
        obs_box_create_boxgroup(context);
    }
}

static void *obs_box_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct obs_box *context = bzalloc(sizeof(struct obs_box));
	context->src = source;
//	obs_data_set_default_obj(settings, "scene", NULL);

    obs_box_update(context, settings);

    if (context->scene != NULL)
        return context;
    else {
        bfree(context);
        return NULL;
    }

	// LEO: direct call obs_box_update will pass not fully initialed source
    // should call obs_source_update if want use member of source
	// obs_source_update(source, settings);
    // printf("box_create: scene = %p\n", obs_source_get_scene(source));

	//UNUSED_PARAMETER(settings);

}

static void obs_box_destroy(void *data)
{
	bfree(data);
}

static obs_properties_t *obs_box_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_color(props, "color",
		obs_module_text("Box.Color"));

	obs_properties_add_int(props, "width",
		obs_module_text("Box.Width"), 0, 4096, 1);

	obs_properties_add_int(props, "height",
		obs_module_text("Box.Height"), 0, 4096, 1);

	return props;
}

static void obs_box_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct obs_box *context = data;

	gs_effect_t    *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t    *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech  = gs_effect_get_technique(solid, "Solid");

	struct vec4 colorVal;
	vec4_from_rgba(&colorVal, context->color);
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_draw_sprite(0, 0, context->width, context->height);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static uint32_t obs_box_getwidth(void *data)
{
	struct obs_box *context = data;
	return context->width;
}

static uint32_t obs_box_getheight(void *data)
{
	struct obs_box *context = data;
	return context->height;
}

static void obs_box_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "fullname", "noname1");
	obs_data_set_default_string(settings, "name", "noname");
	obs_data_set_default_int(settings, "seq", 1);
	obs_data_set_default_double(settings, "x", 0);
	obs_data_set_default_double(settings, "y", 0);

	obs_data_set_default_int(settings, "color", 0x80808080);
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 400);
	obs_data_set_default_int(settings, "synced", 0);
	obs_data_set_default_double(settings, "scene", 0);
}

static bool obs_box_save(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(data);
    /* DO NOT save any box info */
    return false;
}

#if 0 
static void obs_box_mouse_move(void *data, const struct obs_mouse_event *event, bool mouse_leave) 
{ 
  //  if (mouse_leave == false) return;
    
    printf("new box position is %d, %d\n", event->x, event->y);
}
#endif

struct obs_source_info obs_box_info = {
	.id             = "obs_box",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.create         = obs_box_create,
	.destroy        = obs_box_destroy,
	.update         = obs_box_update,
	.save           = obs_box_save,
	.get_name       = obs_box_get_name,
	.get_defaults   = obs_box_defaults,
	.get_width      = obs_box_getwidth,
	.get_height     = obs_box_getheight,
	.video_render   = obs_box_render,
	.get_properties = obs_box_properties
};


OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-tag", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "tag/box/sourceview sources";
}

extern struct obs_source_info obs_box_info;
extern struct obs_source_info sourceview_info;


bool obs_module_load(void)
{
	obs_register_source(&obs_box_info);
	obs_register_source(&sourceview_info);
	return true;
}

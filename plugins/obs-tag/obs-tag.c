#include <obs-module.h>
#include "tft.h"

struct obs_box {
	uint32_t color;

	uint32_t width;
	uint32_t height;

	uint32_t synced;

	obs_source_t *src;
};

static const char *obs_box_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Box");
}

static void obs_box_update(void *data, obs_data_t *settings)
{
	struct obs_box *context = data;
	uint32_t color = (uint32_t)obs_data_get_int(settings, "color");
	uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t height = (uint32_t)obs_data_get_int(settings, "height");
	uint32_t synced = (uint32_t)obs_data_get_int(settings, "synced");

	context->color = color;
	context->width = width;
	context->height = height;
	context->synced = synced;
}

static void *obs_box_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct obs_box *context = bzalloc(sizeof(struct obs_box));
	context->src = source;
    obs_box_update(context, settings);

    printf("box_create: scene = %p\n", obs_source_get_scene(source));

	// LEO: direct call obs_box_update will pass not fully initialed source
    // should call obs_source_update if want use member of source
	// obs_source_update(source, settings);

	//UNUSED_PARAMETER(settings);

	return context;
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
	obs_data_set_default_int(settings, "color", 0x80808080);
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 400);
	obs_data_set_default_int(settings, "synced", 0);
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

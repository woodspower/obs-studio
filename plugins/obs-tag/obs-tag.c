#include <obs-module.h>

struct obs_tag {
	uint32_t color;

	uint32_t width;
	uint32_t height;

	obs_source_t *src;
};

static const char *obs_tag_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Box");
}

static void obs_tag_update(void *data, obs_data_t *settings)
{
	struct obs_tag *context = data;
	uint32_t color = (uint32_t)obs_data_get_int(settings, "color");
	uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t height = (uint32_t)obs_data_get_int(settings, "height");

	context->color = color;
	context->width = width;
	context->height = height;
}

static void *obs_tag_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct obs_tag *context = bzalloc(sizeof(struct obs_tag));
	context->src = source;

	obs_tag_update(context, settings);

	return context;
}

static void obs_tag_destroy(void *data)
{
	bfree(data);
}

static obs_properties_t *obs_tag_properties(void *unused)
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

static void obs_tag_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct obs_tag *context = data;

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

static uint32_t obs_tag_getwidth(void *data)
{
	struct obs_tag *context = data;
	return context->width;
}

static uint32_t obs_tag_getheight(void *data)
{
	struct obs_tag *context = data;
	return context->height;
}

static void obs_tag_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 400);
}

struct obs_source_info obs_tag_info = {
	.id             = "obs_tag",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.create         = obs_tag_create,
	.destroy        = obs_tag_destroy,
	.update         = obs_tag_update,
	.get_name       = obs_tag_get_name,
	.get_defaults   = obs_tag_defaults,
	.get_width      = obs_tag_getwidth,
	.get_height     = obs_tag_getheight,
	.video_render   = obs_tag_render,
	.get_properties = obs_tag_properties
};


OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-tag", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "tag/sourceview sources";
}

extern struct obs_source_info obs_tag_info;
extern struct obs_source_info sourceview_info;

bool obs_module_load(void)
{
	obs_register_source(&obs_tag_info);
	obs_register_source(&sourceview_info);
	return true;
}

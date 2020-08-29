#include <obs-module.h>
#include <graphics/image-file.h>
#include <graphics/graphics.h>
#include <util/dstr.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern int errno ;

#include "segmentation_client.h"
#include "virtual-background.h"

/* clang-format off */

#define SETTING_BLUR                   "blur"
#define SETTING_GROWSHRINK             "growshrink"
#define SETTING_SEGMENTATION_THRESHOLD "segmentation_threshold"


#define TEXT_BLUR                     obs_module_text("Blur")
#define TEXT_GROWSHRINK               obs_module_text("GrowShrink")
#define TEXT_SEGMENTATION_THRESHOLD   obs_module_text("SegmentationThreshold")



/* clang-format on */


static const char *virtual_background_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("VirtualBackgroundName");
}

static void virtual_background_update(void *data, obs_data_t *settings)
{
    struct virtual_background_data *filter = data;

    int blur = (int)obs_data_get_int(settings, SETTING_BLUR);
    int growshrink = (int)obs_data_get_int(settings, SETTING_GROWSHRINK);
    float segmentation_threshold = (float)obs_data_get_double(settings, SETTING_SEGMENTATION_THRESHOLD);

    SegmentationClient_set_parameters(filter->client, segmentation_threshold, blur, growshrink);

    obs_enter_graphics();

    char * effect_path = obs_module_file("virtual-background.effect");
    gs_effect_destroy(filter->effect);
    filter->effect = gs_effect_create_from_file(effect_path, NULL);
    bfree(effect_path);

    obs_leave_graphics();
}

static void virtual_background_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, SETTING_BLUR, 4);
    obs_data_set_default_int(settings, SETTING_GROWSHRINK, 0);
    obs_data_set_default_double(settings, SETTING_SEGMENTATION_THRESHOLD, 0.6);
}

static obs_properties_t *virtual_background_properties(void *data)
{
    UNUSED_PARAMETER(data);

    obs_properties_t *props = obs_properties_create();

    obs_properties_add_float_slider(props, SETTING_SEGMENTATION_THRESHOLD, TEXT_SEGMENTATION_THRESHOLD, 0, 1, 0.05);
    obs_properties_add_int_slider(props, SETTING_GROWSHRINK, TEXT_GROWSHRINK, -50, 50, 1);
    obs_properties_add_int_slider(props, SETTING_BLUR, TEXT_BLUR, 0, 25, 1);
    return props;
}

static void *virtual_background_create(obs_data_t *settings, obs_source_t *context)
{
    struct virtual_background_data *filter =
            bzalloc(sizeof(struct virtual_background_data));
    filter->context = context;
    filter->client = SegmentationClient_create();
    filter->scaler = ImageScaler_create();

    obs_source_update(context, settings);
    return filter;
}

static void virtual_background_destroy(void *data)
{
    struct virtual_background_data *filter = data;

    obs_enter_graphics();
    gs_effect_destroy(filter->effect);
    gs_texture_destroy(filter->target);
    obs_leave_graphics();
    ImageScaler_destroy(filter->scaler);
    SegmentationClient_destroy(filter->client);
    bfree(filter);
}


static void virtual_background_tick(void *data, float seconds)
{
    struct virtual_background_data *filter = data;
    const uint8_t * last_frame = ImageScaler_get_buffer(filter->scaler);
    if (last_frame == NULL) {
        return;
    }

    int height = ImageScaler_get_new_height(filter->scaler);
    int width = ImageScaler_get_new_width(filter->scaler);
    int total_size = ImageScaler_get_buffer_size(filter->scaler);

    SegmentationClient_set_dimensions(filter->client, height, width);

    int rc = SegmentationClient_run_segmentation(filter->client, filter->last_frame_timestamp, last_frame, total_size);
    if (rc != 0) {
        return;
    }

    const uint8_t * mask = SegmentationClient_get_mask(filter->client);
    if (SegmentationClient_get_mask_size(filter->client) != width * height) {
        fprintf(stderr, "Invalid mask size from server: %d. expected %d\n",
                SegmentationClient_get_mask_size(filter->client), width * height);
        return;
    }

    obs_enter_graphics();
    if (filter->target == NULL || filter->target_height != height || filter->target_width != width) {
        if (filter->target != NULL) {
            gs_texture_destroy(filter->target);
        }
        filter->target = gs_texture_create(
                width,
                height,
                GS_A8,
                1,
                (const uint8_t **) &mask,
                0
        );
    }
    gs_texture_set_image(filter->target, mask, width, 0);
    obs_leave_graphics();
}


static void virtual_background_render(void *data, gs_effect_t *effect)
{
    struct virtual_background_data *filter = data;
    obs_source_t *target = obs_filter_get_target(filter->context);
    gs_eparam_t *param;

    if (!target || !filter->target || !filter->effect) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
                                         OBS_ALLOW_DIRECT_RENDERING)) {
        return;
    }

    param = gs_effect_get_param_by_name(filter->effect, "target");
    gs_effect_set_texture(param, filter->target);
    obs_source_process_filter_end(filter->context, filter->effect, 0, 0);
    UNUSED_PARAMETER(effect);
}


static struct obs_source_frame *
virtual_background_filter_video(void *data, struct obs_source_frame *frame)
{
    struct virtual_background_data *filter = data;
    filter->last_frame_timestamp = frame->timestamp;
    ImageScaler_scale_image(filter->scaler, frame);
    return frame;
}



struct obs_source_info virtual_background = {
        .id = "virtual_background",
        .type = OBS_SOURCE_TYPE_FILTER,
        .output_flags = OBS_SOURCE_VIDEO,
        .get_name = virtual_background_get_name,
        .create = virtual_background_create,
        .destroy = virtual_background_destroy,
        .update = virtual_background_update,
        .get_defaults = virtual_background_defaults,
        .get_properties = virtual_background_properties,
        .video_render = virtual_background_render,
        .filter_video = virtual_background_filter_video,
        .video_tick = virtual_background_tick,
};



OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("virtual_background", "en-US")


bool obs_module_load(void)
{
    obs_register_source(&virtual_background);

    return true;
}

void obs_module_unload(void)
{
}
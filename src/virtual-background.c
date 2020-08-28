#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <graphics/image-file.h>
#include <graphics/graphics.h>
#include <util/dstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <libavutil/pixfmt.h>
#include <errno.h>
#include <libswscale/swscale.h>

extern int errno ;

#include "conversions.h"

/* clang-format off */

#define SETTING_BLUR                   "blur"
#define SETTING_GROWSHRINK             "growshrink"
#define SETTING_SEGMENTATION_THRESHOLD "segmentation_threshold"

#define CHECK_PORT_INTERVAL            10000

#define SEGMENTATION_PORT_FILENAME     ".segmentation.port"

#define TEXT_BLUR                     obs_module_text("Blur")
#define TEXT_GROWSHRINK               obs_module_text("GrowShrink")
#define TEXT_SEGMENTATION_THRESHOLD   obs_module_text("SegmentationThreshold")

/* clang-format on */

struct virtual_background_data {
    uint64_t last_frame_timestamp;

    obs_source_t *context;
    gs_effect_t *effect;

    gs_texture_t *target;
    int target_height;
    int target_width;

    uint8_t *last_frame;
    int last_frame_height;
    int last_frame_width;

    struct SwsContext *sws_context;

    int client_port;
    int client_socket;

    uint64_t last_port_timestamp;
    float segmentation_threshold;
    int blur;
    int growshrink;
};

void update_mask(
        uint8_t * mask,
        struct virtual_background_data * filter);

void handle_socket_failure(struct virtual_background_data * filter);


char REQUEST_HEADER[] = {-18, 97, -66, -60, 56, -46, 86, -87};
char RESPONSE_HEADER[] = {80, 119, 61, -38, -56, 125, 93, -105};


uint8_t *get_frame_buffer(struct virtual_background_data * filter, int height, int width)
{
    if (filter->last_frame != NULL && filter->last_frame_height == height && filter->last_frame_width == width) {
        return filter->last_frame;
    }
    if (filter->last_frame != NULL) {
        bfree(filter->last_frame);
    }
    filter->last_frame = (uint8_t *)bzalloc(width * height * 3);
    filter->last_frame_height = height;
    filter->last_frame_width = width;
    return filter->last_frame;
}

static int get_segmentation_port() {
    char* tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }
    char* segmentation_path = (char *)malloc(strlen(tmpdir) + 1 + strlen(SEGMENTATION_PORT_FILENAME) + 1);
    strcpy(segmentation_path, tmpdir);
    strcat(segmentation_path, "/");
    strcat(segmentation_path, SEGMENTATION_PORT_FILENAME);

    FILE *file = fopen(segmentation_path, "r");
    free(segmentation_path);

    if (file == NULL) {
        return -1;
    }
    int port;
    size_t rc = fread(&port, 1, sizeof(int), file);
    if (rc != sizeof(int)) {
        port = -1;
    }
    fclose(file);
    return port;
}

static void connect_to_node(struct virtual_background_data * data) {
    if (data->client_socket != -1) {
        close(data->client_socket);
        data->client_socket = -1;
    }
    if (data->client_port == -1) {
        return;
    }
    struct sockaddr_in server_addr, client_addr;
    data->client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data->client_socket == -1) {
        return;
    }
    int trueval = 1;
    setsockopt(data->client_socket, SOL_SOCKET, SO_REUSEADDR, &trueval ,sizeof(int));
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(data->client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close(data->client_socket);
        data->client_socket = -1;
        return;
    }

    if (setsockopt(data->client_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close(data->client_socket);
        data->client_socket = -1;
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(data->client_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    struct hostent *server = gethostbyname("localhost");
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    if (connect(data->client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(data->client_socket);
        data->client_socket = -1;
        return;
    }
}

static const char *virtual_background_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("VirtualBackgroundName");
}

static void virtual_background_update(void *data, obs_data_t *settings)
{
    struct virtual_background_data *filter = data;

    filter->blur = obs_data_get_int(settings, SETTING_BLUR);
    filter->growshrink = obs_data_get_int(settings, SETTING_GROWSHRINK);
    filter->segmentation_threshold = obs_data_get_double(settings, SETTING_SEGMENTATION_THRESHOLD);

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
    filter->client_socket = -1;
    filter->last_frame = NULL;

    obs_source_update(context, settings);
    return filter;
}

static void virtual_background_destroy(void *data)
{
    struct virtual_background_data *filter = data;

    obs_enter_graphics();
    gs_effect_destroy(filter->effect);
    //gs_image_file_free(&filter->image);
    gs_texture_destroy(filter->target);
    obs_leave_graphics();
    if (filter->last_frame != NULL) {
        bfree(filter->last_frame);
    }

    bfree(filter);
}


static void virtual_background_tick(void *data, float seconds)
{
    struct virtual_background_data *filter = data;
    if (filter->last_frame == NULL) {
        return;
    }
    int height = filter->last_frame_height;
    int width = filter->last_frame_width;

    if (filter->last_port_timestamp == 0 || (filter->last_frame_timestamp - filter->last_port_timestamp) >= CHECK_PORT_INTERVAL) {
        int segmentation_port = get_segmentation_port();
        if (segmentation_port != filter->client_port) {
            filter->client_port = -1;
            close(filter->client_socket);
            filter->client_socket = -1;
        }
        filter->client_port = segmentation_port;
        filter->last_port_timestamp = filter->last_frame_timestamp;
    }
    if (filter->client_socket < 0) {
        connect_to_node(filter);
    }
    int total_size = 8 + 4 + 4 + 2 + 2 + height * width * 3;

    int written = write(filter->client_socket, &total_size, sizeof(int));
    if (written != sizeof(int)) {
        return;
    }
    written = write(filter->client_socket, &REQUEST_HEADER, 8);
    if (written != 8) {
        return;
    }
    written = write(filter->client_socket, &filter->segmentation_threshold, sizeof(float));
    if (written != 4) {
        return;
    }
    uint16_t sheight, swidth, sblur, sgrowshrink;
    sheight = (uint16_t) height;
    swidth = (uint16_t) width;
    sblur = (uint16_t) filter->blur;
    sgrowshrink = (uint16_t) filter->growshrink;

    written = write(filter->client_socket, &sheight, 2);
    if (written != 2) {
        return;
    }
    written = write(filter->client_socket, &swidth, 2);
    if (written != 2) {
        return;
    }
    written = write(filter->client_socket, &sblur, 2);
    if (written != 2) {
        return;
    }
    written = write(filter->client_socket, &sgrowshrink, 2);
    if (written != 2) {
        return;
    }

    written = write(filter->client_socket, filter->last_frame, height * width * 3);

    int32_t response_size;
    int amountRead = recv(filter->client_socket, &response_size, 4, 0);
    if (amountRead < 4) {
        handle_socket_failure(filter);
        return;
    }
    if (response_size < 1) {
        handle_socket_failure(filter);
        return;
    }
    char response_header[8];
    amountRead = recv(filter->client_socket, &response_header, 8, 0);
    if (amountRead < 8) {
        handle_socket_failure(filter);
        return;
    }
    if (strncmp(&response_header, &RESPONSE_HEADER, 8) != 0) {
        handle_socket_failure(filter);
        return;
    }
    uint8_t * mask = bzalloc(response_size);
    int total_read = 0;
    while (total_read < response_size) {
        amountRead = recv(filter->client_socket, mask + total_read, response_size, 0);
        total_read += amountRead;
    }
    obs_enter_graphics();
    update_mask(mask, filter);
    obs_leave_graphics();
    bfree(mask);
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
    uint8_t *buffer = get_frame_buffer(filter, frame->height, frame->width);
//
//    filter->sws_context = sws_getCachedContext(
//            filter->sws_context,
//            frame->width, frame->height,
//            AV_PIX_FMT_YVYU422,
//            frame->width, frame->height,
//            AV_PIX_FMT_BGR24,
//            0, 0, 0, 0);
//    int height = sws_scale(filter->sws_context, (const uint8_t *const *)frame->data,
//              (const int *)frame->linesize, 0, frame->height,
//              &buffer, (const int *)frame->linesize);
    //printf("Got height %d from sws_scale\n", height);
    to_bgr(buffer, frame);
    return frame;
}


void update_mask(
        uint8_t * mask,
        struct virtual_background_data * filter)
{
    int height = filter->last_frame_height;
    int width = filter->last_frame_width;
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
        gs_texture_set_image(filter->target, mask, width, 0);
    } else {
        gs_texture_set_image(filter->target, mask, width, 0);
    }
}


void handle_socket_failure(struct virtual_background_data * filter)
{
    if (filter->client_socket != -1) {
        close(filter->client_socket);
        filter->client_socket = -1;
        filter->last_port_timestamp = 0;
    }
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
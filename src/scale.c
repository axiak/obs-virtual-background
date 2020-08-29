#include <obs-module.h>
#include <libswscale/swscale.h>

#include "scale.h"

#define MAX_WIDTH 640

static inline enum AVPixelFormat get_ffmpeg_video_format(enum video_format format)
{
    switch (format) {
        case VIDEO_FORMAT_I420:
            return AV_PIX_FMT_YUV420P;
        case VIDEO_FORMAT_NV12:
            return AV_PIX_FMT_NV12;
        case VIDEO_FORMAT_YUY2:
            return AV_PIX_FMT_YUYV422;
        case VIDEO_FORMAT_UYVY:
            return AV_PIX_FMT_UYVY422;
        case VIDEO_FORMAT_RGBA:
            return AV_PIX_FMT_RGBA;
        case VIDEO_FORMAT_BGRA:
        case VIDEO_FORMAT_BGRX:
            return AV_PIX_FMT_BGRA;
        case VIDEO_FORMAT_Y800:
            return AV_PIX_FMT_GRAY8;
        case VIDEO_FORMAT_I444:
            return AV_PIX_FMT_YUV444P;
        case VIDEO_FORMAT_BGR3:
            return AV_PIX_FMT_BGR24;
        case VIDEO_FORMAT_I422:
            return AV_PIX_FMT_YUV422P;
        case VIDEO_FORMAT_I40A:
            return AV_PIX_FMT_YUVA420P;
        case VIDEO_FORMAT_I42A:
            return AV_PIX_FMT_YUVA422P;
        case VIDEO_FORMAT_YUVA:
            return AV_PIX_FMT_YUVA444P;
        case VIDEO_FORMAT_NONE:
        case VIDEO_FORMAT_YVYU:
        case VIDEO_FORMAT_AYUV:
            /* not supported by FFmpeg */
            return AV_PIX_FMT_NONE;
    }

    return AV_PIX_FMT_NONE;
}


const int ImageScaler_scale_image(ImageScaler *scaler, const struct obs_source_frame *frame)
{
    int width = frame->width;
    int height = frame->height;

    if (width <= MAX_WIDTH) {
        scaler->new_width = width;
        scaler->new_height = height;
    } else {
        scaler->new_width = MAX_WIDTH;
        scaler->new_height = (int) (((double) MAX_WIDTH) / width * height);
    }

    struct SwsContext * sws_context = sws_getCachedContext(scaler->scale_context,
                                       width, height, get_ffmpeg_video_format(frame->format),
                                       scaler->new_width, scaler->new_height, AV_PIX_FMT_BGR24,
                                       SWS_BICUBIC, NULL, NULL, NULL);
    scaler->scale_context = sws_context;

    if (sws_context == NULL) {
        return 1;
    }

    int buffer_size = scaler->new_width * scaler->new_height * 3;
    if (scaler->buffer == NULL || scaler->buffer_size != buffer_size) {
        if (scaler->buffer != NULL) {
            bfree(scaler->buffer);
        }
        scaler->buffer = bzalloc(buffer_size);
        scaler->buffer_size = buffer_size;
        if (scaler->buffer == NULL) {
            return 1;
        }
    }

    int new_stride[] = {scaler->new_width * 3, 0, 0};

    sws_scale(
            sws_context,                            /* swsContext c*/
            (const uint8_t * const *)frame->data,        /* srcSlice[] */
            (const int *)frame->linesize,           /* srcStride[] */
            0,                             /* srcSliceY */
              height,                               /* srcSliceH */
              (uint8_t **)&scaler->buffer,           /* dst[] */
              new_stride                            /* dstStride[] */
              );

    return 0;
}


const uint8_t * ImageScaler_get_buffer(ImageScaler *scaler)
{
    return scaler->buffer;
}

int ImageScaler_get_new_height(ImageScaler *scaler)
{
    return scaler->new_height;
}

int ImageScaler_get_new_width(ImageScaler *scaler)
{
    return scaler->new_width;
}

int ImageScaler_get_buffer_size(ImageScaler *scaler)
{
    return scaler->buffer_size;
}

ImageScaler * ImageScaler_create()
{
    ImageScaler * result = (ImageScaler *)bzalloc(sizeof(ImageScaler));
    if (result == NULL) {
        return NULL;
    }
    result->new_height = 0;
    result->new_width = 0;
    result->old_height = 0;
    result->old_width = 0;
    result->buffer = NULL;
    result->buffer_size = 0;
    result->scale_context = NULL;
}

void ImageScaler_destroy(ImageScaler *scaler)
{
    if (scaler != NULL) {
        if (scaler->buffer != NULL) {
            bfree(scaler->buffer);
            scaler->buffer = NULL;
        }
        if (scaler->scale_context != NULL) {
            sws_freeContext(scaler->scale_context);
            scaler->scale_context = NULL;
        }
        bfree(scaler);
    }
}

#include <obs-module.h>
#include <stdio.h>
#include "conversions.h"

static void yuy2_to_bgr(uint8_t *buf, const struct obs_source_frame *frame);

static inline int clamp(int lower, int value, int upper)
{
    if (value > upper) {
        return upper;
    } else if (value < lower) {
        return lower;
    } else {
        return value;
    }
}

static inline uint8_t c0255(double v)
{
    if (v > 255) {
        return 255;
    } else if (v < 0) {
        return 0;
    } else {
        return (uint8_t)v;
    }
}

static inline double yuv2r(int y, int u, int v)
{
    UNUSED_PARAMETER(u);
    return 1.164 * y + 1.793 * v;
}

static inline double yuv2g(int y, int u, int v)
{
    return 1.164 * y - 0.213 * u - 0.533 * v;
}

static inline double yuv2b(int y, int u, int v)
{
    UNUSED_PARAMETER(v);
    return 1.164 * y + 2.112 * u;
}

int to_bgr(uint8_t *buf, const struct obs_source_frame *frame)
{
    switch (frame->format)
    {
        case VIDEO_FORMAT_YUY2:
            yuy2_to_bgr(buf, frame);
            return 0;
        case VIDEO_FORMAT_BGR3:
            memcpy(buf, frame->data[0], frame->width * frame->height * 3);
            return 0;
        default:
            printf("got video fromat %d\n", frame->format);
            return -1;
    }
}

static void yuy2_to_bgr(uint8_t *buf, const struct obs_source_frame *frame)
{
    uint8_t* yuvy = frame->data[0];

    for (int i = 0; i < frame->height; ++i) {
        for (int j = 0; j < frame->width; j += 2) {
            int index = i * frame->width + j;
            int index2 = index * 2;
            int index3 = index * 3;

            int y0 = clamp(16, yuvy[index2 + 0], 235) - 16;
            int u = clamp(16, yuvy[index2 + 1], 240) - 128;
            int y1 = clamp(16, yuvy[index2 + 2], 235) - 16;
            int v = clamp(16, yuvy[index2 + 3], 240) - 128;

            buf[index3 + 0] = c0255(yuv2b(y0, u, v));
            buf[index3 + 1] = c0255(yuv2g(y0, u, v));
            buf[index3 + 2] = c0255(yuv2r(y0, u, v));
            buf[index3 + 3] = c0255(yuv2b(y1, u, v));
            buf[index3 + 4] = c0255(yuv2g(y1, u, v));
            buf[index3 + 5] = c0255(yuv2r(y1, u, v));
        }
    }
}


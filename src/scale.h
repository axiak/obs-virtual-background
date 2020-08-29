//
// Created by axiak on 8/28/20.
//

#ifndef OBS_VIRTUAL_BACKGROUND_SCALE_H
#define OBS_VIRTUAL_BACKGROUND_SCALE_H

#include <libswscale/swscale.h>

typedef struct {
    int new_height;
    int new_width;
    int old_width;
    int old_height;

    uint8_t *buffer;
    size_t buffer_size;

    struct SwsContext * scale_context;
} ImageScaler;

ImageScaler * ImageScaler_create();
void ImageScaler_destroy(ImageScaler *scaler);
int ImageScaler_get_new_height(ImageScaler *scaler);
int ImageScaler_get_new_width(ImageScaler *scaler);

int ImageScaler_get_buffer_size(ImageScaler *scaler);
const uint8_t * ImageScaler_get_buffer(ImageScaler *scaler);

const int ImageScaler_scale_image(ImageScaler *scaler, const struct obs_source_frame *frame);


#endif //OBS_VIRTUAL_BACKGROUND_SCALE_H
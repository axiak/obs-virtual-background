//
// Created by axiak on 8/28/20.
//

#ifndef OBS_VIRTUAL_BACKGROUND_VIRTUAL_BACKGROUND_H
#define OBS_VIRTUAL_BACKGROUND_VIRTUAL_BACKGROUND_H

#include <obs-module.h>
#include "scale.h"
#include "segmentation_client.h"

struct virtual_background_data {
    uint64_t last_frame_timestamp;

    obs_source_t *context;
    gs_effect_t *effect;

    gs_texture_t *target;
    int target_height;
    int target_width;

    SegmentationClient  *client;
    ImageScaler *scaler;
};


#endif //OBS_VIRTUAL_BACKGROUND_VIRTUAL_BACKGROUND_H

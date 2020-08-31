#ifndef OBS_VIRTUAL_BACKGROUND_SEGMENTATION_THREAD_H
#define OBS_VIRTUAL_BACKGROUND_SEGMENTATION_THREAD_H

#include <stdint.h>
#include <pthread.h>

#include "segmentation_client.h"
#include "imgarray.h"

typedef struct {
    pthread_t thread_id;
    pthread_mutex_t mutex;
    ImgArray * bgr;
    pthread_mutex_t data_mutex;
    uint64_t buffer_counter;
    uint8_t is_running;
    SegmentationClient * client;

    ImgArray * mask;
    uint64_t timestamp;
} SegmentationThread;


SegmentationThread * SegmentationThread_create();
void SegmentationThread_destroy(SegmentationThread * self);

void SegmentationThread_set_dimensions(SegmentationThread * self, int height, int width);
void SegmentationThread_set_parameters(SegmentationThread * self, float segmentation_threshold, int blur, int growshrink);
void SegmentationThread_update_buffer(SegmentationThread * self, uint64_t timestamp, const uint8_t * buffer, int buffer_size);
int SegmentationThread_get_mask(SegmentationThread * self, ImgArray * dst);


#endif //OBS_VIRTUAL_BACKGROUND_SEGMENTATION_THREAD_H

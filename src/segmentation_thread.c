#include <pthread.h>

#include <obs-module.h>
#include <time.h>

#include "segmentation_thread.h"
#include "segmentation_client.h"
#include "imgarray.h"

void * run_thread(void *thread_ptr);
void lock(SegmentationThread * self);
void unlock(SegmentationThread * self);
void sleepthread();


typedef struct {
    uint64_t timestamp;
    ImgArray * bgr;
    uint64_t buffer_counter;
    uint64_t last_buffer_counter;
    int8_t is_running;
} local_data;


SegmentationThread * SegmentationThread_create()
{
    SegmentationThread * self = (SegmentationThread *)bzalloc(sizeof(SegmentationThread));
    if (!self) {
        return NULL;
    }
    self->thread_id = -1;
    self->client = SegmentationClient_create();
    if (!self->client) {
        goto err;
    }
    self->bgr = ImgArray_create();
    self->mask = ImgArray_create();
    self->buffer_counter = 0;
    self->is_running = 1;
    if (pthread_create(&(self->thread_id), NULL, run_thread, (void *)self)) {
        goto err;
    }
#ifdef _GNU_SOURCE
    pthread_setname_np(self->thread_id, "virtual-background-segmentation");
#endif

    pthread_mutex_init(&(self->mutex), NULL);
    return self;

    err:
    SegmentationThread_destroy(self);
    return NULL;
}


void SegmentationThread_destroy(SegmentationThread * self)
{
    if (!self) {
        return;
    }
    lock(self);
    self->is_running = 0;
    unlock(self);
    pthread_join(self->thread_id, NULL);
    if (self->bgr) {
        ImgArray_destroy(self->bgr);
    }
    if (self->mask) {
        ImgArray_destroy(self->mask);
    }
    if (self->client) {
        SegmentationClient_destroy(self->client);
    }
    bfree(self);
}


void SegmentationThread_set_dimensions(SegmentationThread * self, int height, int width)
{
    lock(self);
    SegmentationClient_set_dimensions(self->client, height, width);
    unlock(self);
}


void SegmentationThread_set_parameters(SegmentationThread * self, float segmentation_threshold, int blur, int growshrink)
{
    lock(self);
    SegmentationClient_set_parameters(self->client, segmentation_threshold, blur, growshrink);
    unlock(self);
}


void SegmentationThread_update_buffer(SegmentationThread * self, uint64_t timestamp, const uint8_t * bgr, int buffer_size)
{
    lock(self);
    self->timestamp = timestamp;
    self->buffer_counter++;
    ImgArray_copy_from_raw_buffer(self->bgr, bgr, buffer_size);
    unlock(self);
}


void * run_thread(void *ptr)
{
    SegmentationThread * self = (SegmentationThread *)ptr;
    SegmentationClient * client;
    local_data local_data = {
            .timestamp = 0,
            .bgr = NULL,
            .buffer_counter = 0,
            .is_running = 1
    };
    local_data.bgr = ImgArray_create();

    while (1) {
        lock(self);
        local_data.is_running = self->is_running;
        local_data.buffer_counter = self->buffer_counter;
        unlock(self);

        if (!local_data.is_running) {
            goto end;
        }
        if (local_data.last_buffer_counter == local_data.buffer_counter) {
            sleepthread();
            continue;
        }
        local_data.buffer_counter = local_data.last_buffer_counter;

        lock(self);
        if (!ImgArray_get_buffer(self->bgr)) {
            unlock(self);
            sleepthread();
            continue;
        }

        if (ImgArray_copy_from_array(local_data.bgr, self->bgr)) {
            unlock(self);
            goto end;
        }
        client = self->client;
        local_data.timestamp = self->timestamp;
        unlock(self);

        int rc = SegmentationClient_run_segmentation(
                client,
                local_data.timestamp,
                ImgArray_get_buffer(local_data.bgr),
                ImgArray_get_size(local_data.bgr)
        );
        if (rc) {
            sleepthread();
            continue;
        }

        lock(self);
        rc = ImgArray_copy_from_raw_buffer(
                self->mask,
                SegmentationClient_get_mask(client),
                SegmentationClient_get_mask_size(client)
        );
        unlock(self);
        if (rc) {
            goto end;
        }

        sleepthread();
    }

end:
    if (local_data.bgr) {
        ImgArray_destroy(local_data.bgr);
    }
}


int SegmentationThread_get_mask(SegmentationThread * self, ImgArray * dst)
{
    lock(self);
    int rc = ImgArray_copy_from_array(dst, self->mask);
    unlock(self);
    return rc;
}


void lock(SegmentationThread * self)
{
    pthread_mutex_lock(&(self->mutex));
}

void unlock(SegmentationThread * self)
{
    pthread_mutex_unlock(&(self->mutex));
}

void sleepthread()
{
    nanosleep((const struct timespec[]){{0, 10000000L}}, NULL);
}

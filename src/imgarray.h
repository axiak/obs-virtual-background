#ifndef OBS_VIRTUAL_BACKGROUND_IMGARRAY_H
#define OBS_VIRTUAL_BACKGROUND_IMGARRAY_H

#include <stdint.h>
#include <stdlib.h>


typedef struct {
    uint8_t * buffer;
    size_t size;
} ImgArray;


ImgArray * ImgArray_create();
void ImgArray_destroy(ImgArray * self);
uint8_t * ImgArray_ensure_buffer(ImgArray * self, size_t size);
uint8_t * ImgArray_get_buffer(ImgArray *self);
size_t ImgArray_get_size(ImgArray *self);
int ImgArray_copy_from_raw_buffer(ImgArray *self, const uint8_t * other, size_t size);
int ImgArray_copy_from_array(ImgArray *self, ImgArray *other);

#endif //OBS_VIRTUAL_BACKGROUND_IMGARRAY_H

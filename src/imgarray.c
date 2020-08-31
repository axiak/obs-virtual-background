#include <obs-module.h>

#include "imgarray.h"


ImgArray * ImgArray_create()
{
    ImgArray * arr = (ImgArray *)bzalloc(sizeof(ImgArray));
    if (!arr) {
        return NULL;
    }
    arr->buffer = NULL;
    arr->size = 0;
}


void ImgArray_destroy(ImgArray * self)
{
    if (!self) {
        return;
    }
    if (self->buffer) {
        bfree(self->buffer);
    }
    bfree(self);
}

int ImgArray_copy_from_raw_buffer(ImgArray *self, const uint8_t * other, size_t size)
{
    uint8_t * target = ImgArray_ensure_buffer(self, size);
    if (!target) {
        return 1;
    }
    memcpy(target, other, size);
    return 0;
}

int ImgArray_copy_from_array(ImgArray *self, ImgArray *other)
{
    if (!other->buffer) {
        return 1;
    }
    return ImgArray_copy_from_raw_buffer(self, other->buffer, other->size);
}

uint8_t * ImgArray_get_buffer(ImgArray *self)
{
    return self->buffer;
}

size_t ImgArray_get_size(ImgArray *self)
{
    return self->size;
}

uint8_t * ImgArray_ensure_buffer(ImgArray * self, size_t size)
{
    if (self->buffer && self->size == size) {
        return self->buffer;
    }
    if (self->buffer) {
        bfree(self->buffer);
    }
    self->buffer = (uint8_t *)bzalloc(size);
    self->size = size;
    return self->buffer;
}
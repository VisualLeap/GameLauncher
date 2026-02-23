// stb_image_resize2_impl.cpp - Implementation file for stb_image_resize2
// This file includes the stb library implementation exactly once

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_MALLOC(size,context) ((void)(context), malloc(size))
#define STBIR_FREE(ptr,context) ((void)(context), free(ptr))
#include "stb_image_resize2.h"

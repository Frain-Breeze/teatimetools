#pragma once

#include <stdint.h>
#include <vector>

struct KCOL {
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
static_assert(sizeof(KCOL) == 4);

struct KCOLF {
    float a;
    float r;
    float g;
    float b;
};
static_assert(sizeof(KCOLF) == 4 * 4);

float kmeans(std::vector<KCOL>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOL>& out_palette, int k_count, int training_rounds);

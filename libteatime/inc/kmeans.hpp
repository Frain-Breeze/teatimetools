#pragma once

#include <stdint.h>
#include <vector>

struct KCOL {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};
static_assert(sizeof(KCOL) == 4);

struct KCOLF {
    float r;
    float g;
    float b;
    float a;
};
static_assert(sizeof(KCOLF) == 4 * 4);

float kmeans(std::vector<KCOL>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOL>& out_palette, int k_count, int training_rounds = 0, int max_rounds = 0);

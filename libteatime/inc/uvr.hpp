#pragma once

#include <filesystem>
#include <stdint.h>

bool uvr_extract(const std::filesystem::path& fileIn, const std::filesystem::path& fileOut);
bool uvr_repack(const std::filesystem::path& fileIn, const std::filesystem::path& fileOut, int palette_colors = 256);

struct COLOR {
	uint8_t R, G, B, A;
};
static_assert(sizeof(COLOR) == 4, "COLOR struct is not the correct size");
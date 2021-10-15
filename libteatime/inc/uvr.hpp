#pragma once

#include <filesystem>

bool uvr_extract(const std::filesystem::path& fileIn, const std::filesystem::path& fileOut);
bool uvr_repack(const std::filesystem::path& fileIn, const std::filesystem::path& fileOut);

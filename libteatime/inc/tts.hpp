#pragma once

#include <filesystem>

bool tts_extract(std::filesystem::path fileIn, std::filesystem::path dirOut);
bool tts_repack(std::filesystem::path dirIn, std::filesystem::path fileOut);
bool conversation_extract(std::filesystem::path fileIn, std::filesystem::path texIn, std::filesystem::path fileOut);

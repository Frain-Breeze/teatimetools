#pragma once

bool tts_extract(fs::path fileIn, fs::path dirOut);
bool tts_repack(fs::path dirIn, fs::path fileOut);
bool conversation_extract(fs::path fileIn, fs::path texIn, fs::path fileOut);

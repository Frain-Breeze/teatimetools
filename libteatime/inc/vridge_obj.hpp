#pragma once

#include <teaio_file.hpp>

#include <vector>

class VridgeObj {
public:
	bool open(Tea::File& file);
	bool export_entry(Tea::File& file, int entry); //decompresses and then stitches all entries together
	
	class Entry {
	public:
		bool open(Tea::File& file, bool has_palette);
		
		struct SubEntry {
			Tea::File* data = nullptr;
		};
		std::vector<SubEntry> _subentries;
	};
	std::vector<Entry> _entries;
	
	struct Color {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
	static_assert(sizeof(Color) == 4, "Color struct should be packed to 4 bytes but it's not");
	
	std::vector<std::vector<Color>> _palettes;
};
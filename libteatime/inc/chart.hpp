#pragma once

#include <vector>

class Chart {
public:
	enum class Type {
		none = 0,
		hit = 1,
		hold_start = 2,
		hold_trail = 3
	};
	
	class Track {
	public:
		bool load_from_ksd(const char* path);
		
		
		
		std::vector<Type[8]> notes;
		bool active = false;
	} tracks[2][5];
	
	bool load_from_sm(const char* path);
};
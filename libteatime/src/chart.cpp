#include "chart.hpp"

#include <string_view>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdio.h>

#include <logging.hpp>

bool Chart::Track::load_from_ksd(const char* path) {
	LOGERR("bad unimplemented");
	return false;
}

bool Chart::load_from_sm(const char* path) {
	FILE* fi = fopen(path, "r");
	if(!fi) { LOGERR("couldn't open file %s, skipping...", path); return false; }
	
	LOGERR("bad, not finished\n");
	return false;
	
	std::string content;
	fseek(fi, 0, SEEK_END);
	content.resize(ftell(fi));
	fseek(fi, 0, SEEK_SET);
	fread(content.data(), 1, content.size(), fi);
	
	size_t progress = 0;
	while(progress < content.size()) {
		size_t start = content.find_first_of('#', progress);
		size_t end = content.find_first_of(';', progress);
		if(start == content.npos || end == content.npos) { LOGERR("couldn't parse haha"); goto return_bad; }
		if(start >= content.size() || end >= content.size()) { LOGERR("last part too beeg"); goto return_bad; }
		std::string part = content.substr(start, end);
		if(part.rfind("#NOTES:", 0) == 0) {
			LOGINF("found notes");
			std::stringstream sspart(part);
			std::string segment;
			while(std::getline(sspart, segment, ':')) {
				segment.erase(0, segment.find_first_not_of("\n\r\t "));
				//segment.erase(std::remove(segment.begin(), segment.end(), '\n'), segment.end());
				LOGINF("segment: %s", segment.c_str());
				
				
			}
		}
		progress = end + 1;
	}
	
	fclose(fi);
	return true;
return_bad:
	fclose(fi);
	return false;
}
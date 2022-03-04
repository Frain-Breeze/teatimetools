#pragma once

#include <string>
#include <vector>
#include "teaio_file.hpp"

class DigitalcuteArchive {
public:
	bool open_bin(Tea::File& infile);
	bool write_dir(const std::string& outpath);
	
private:
	struct Entry {
		uint16_t ID;
		std::string name1;
		std::string name2;
		uint32_t type;
		Tea::File* data = nullptr;
	};
	std::vector<Entry> _filetable;
};
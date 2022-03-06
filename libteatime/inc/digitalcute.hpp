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
		int group = -1;
		enum class Type : uint32_t {
			folder = 16,
			file1 = 32,
			file2 = 128,
		} type;
		Tea::File* data = nullptr;
	};
	std::vector<Entry> _filetable;
};
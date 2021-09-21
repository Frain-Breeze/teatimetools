#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <string.h>
namespace fs = std::filesystem;

#include "logging.hpp"

struct fmdx_entry {
	char name[129];
	uint32_t offset;
	uint32_t size;
	std::vector<uint8_t> data;
};

bool fmdx_extract(fs::path rootDirIn, fs::path pathToIn, fs::path rootDirOut) {
	fs::path pacPath = rootDirOut;
	pacPath /= pathToIn.parent_path();
	fs::create_directories(pacPath);
	pacPath /= pathToIn.filename() += ".package.txt";
	LOGINF("out path: %s", pacPath.string().c_str());

	fs::path fullInPath = rootDirIn;
	fullInPath /= pathToIn;

	FILE* fi = fopen(fullInPath.u8string().c_str(), "rb");

	uint32_t magic;
	fread(&magic, 4, 1, fi);
	if (magic != 'XDMF') {
		return false;
	}

	fseek(fi, 8, SEEK_SET);
	uint32_t fileCount = 0;
	fread(&fileCount, 4, 1, fi);
	LOGINF("filecount: %d", (int)fileCount);

	FILE* fpac = fopen(pacPath.u8string().c_str(), "wb");

	fseek(fi, 84, SEEK_SET);
	fmdx_entry entry;
	for (size_t i = 0; i < fileCount; i++) {
		entry.name[128] = 0;
		fread(entry.name, 1, 128, fi);
		fread(&entry.offset, 4, 1, fi);
		fread(&entry.size, 4, 1, fi);
		fseek(fi, 8, SEEK_CUR);
		size_t offs = ftell(fi);
		
		LOGINF("offset: %8d, size: %8d, name: %s", entry.offset, entry.size, entry.name);

		fseek(fi, entry.offset, SEEK_SET);

		entry.data.resize(entry.size);
		fread(entry.data.data(), 1, entry.size, fi);
		fs::path fullOutPath = rootDirOut;
		fullOutPath /= fs::u8path(entry.name);
		fs::create_directories(fullOutPath.parent_path());
		FILE* fo = fopen(fullOutPath.u8string().c_str(), "wb");
		fwrite(entry.data.data(), 1, entry.size, fo);
		fclose(fo);
		fseek(fi, offs, SEEK_SET);

		fwrite(entry.name, 1, strlen(entry.name), fpac);
		fwrite("\n", 1, 1, fpac);
	}

	fclose(fpac);

	return true;
}

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

bool fmdx_repack(fs::path rootDirIn, fs::path pathToIn, fs::path rootDirOut){
    fs::path pacPath = rootDirIn;
	pacPath /= pathToIn;

    //TODO: clean up finding the package name
    if(pathToIn.filename().u8string().find(".package.txt") != std::string::npos){
        std::string outPart = pathToIn.u8string().substr(0, pathToIn.u8string().find(".package.txt"));
        fs::path outPath = rootDirOut;
        outPath /= outPart;
        //LOGINF("out path: %s", outPath.u8string().c_str());


        std::vector<std::string> entries;
        FILE* fpac = fopen(pacPath.u8string().c_str(), "r");
        char pathbuf[2048];
        while(fgets(pathbuf, 2048, fpac)){
            pathbuf[strcspn(pathbuf,"\n")] = 0;
            entries.push_back(pathbuf);
        }
        fclose(fpac);

        fs::create_directories(outPath.parent_path());
        FILE* fo = fopen(outPath.u8string().c_str(), "wb");
        fwrite("FMDX", 4, 1, fo);
        fwrite("LOLI", 4, 1, fo); //later fill this with offset
        uint32_t fileCount = entries.size();
        fwrite(&fileCount, 4, 1, fo);
        fwrite("\1", 1, 1, fo);
        fseek(fo, 84 + (144 * entries.size()), SEEK_SET);

		size_t total_entry_size = 0;
		
        for(int i = 0; i < entries.size(); i++){
            fs::path finPath = rootDirIn;
            finPath /= entries[i];



            FILE* fi = fopen(finPath.u8string().c_str(), "rb");
            fseek(fi, 0, SEEK_END);
            uint32_t fisize = ftell(fi);
            fseek(fi, 0, SEEK_SET);
            uint8_t* fidata = (uint8_t*)malloc(fisize);
            fread(fidata, fisize, 1, fi);

            while(ftell(fo) % 16) { fseek(fo, 1, SEEK_CUR); } //align to 16 bytes
            uint32_t entry_offset = ftell(fo);

            //write info to header
            fseek(fo, 84 + (144 * i), SEEK_SET);
            char name[128];
            snprintf(name, 128, "%s", entries[i].c_str());
            fwrite(name, 128, 1, fo);
            fwrite(&entry_offset, 4, 1, fo);
            fwrite(&fisize, 4, 1, fo);
            fseek(fo, entry_offset, SEEK_SET);

            //write actual file data
            fwrite(fidata, fisize, 1, fo);

            free(fidata);
            fclose(fi);

			total_entry_size += fisize;
            LOGVER("added entry at %10d with size %8d, named %s", entry_offset, fisize, name);
        }

        uint32_t total_size = ftell(fo);
        uint32_t data_size = total_size - 84 - (144 * entries.size());
        fseek(fo, 4, SEEK_SET);
        fwrite(&data_size, 4, 1, fo);

        fclose(fo);

		LOGOK("saved archive, packing %d files with a combined size of %d bytes", entries.size(), total_entry_size);
    }else{
        LOGWAR("couldn't find .package.txt in %s, skipping!", pathToIn.filename().u8string().c_str());
    }


    return true;
}

bool fmdx_extract(fs::path rootDirIn, fs::path pathToIn, fs::path rootDirOut) {
	fs::path pacPath = rootDirOut;
	pacPath /= pathToIn.parent_path();
	fs::create_directories(pacPath);
	pacPath /= pathToIn.filename() += ".package.txt";
	LOGVER("out path: %s", pacPath.string().c_str());

	fs::path fullInPath = rootDirIn;
	fullInPath /= pathToIn;

	FILE* fi = fopen(fullInPath.u8string().c_str(), "rb");

	uint32_t magic;
	fread(&magic, 4, 1, fi);

	if (magic != 0x58444D46) { //FMDX
        LOGERR("magic wasn't FMDX");
        fclose(fi);
		return false;
	}

	fseek(fi, 8, SEEK_SET);
	uint32_t fileCount = 0;
	fread(&fileCount, 4, 1, fi);
	LOGVER("filecount: %d", (int)fileCount);

	FILE* fpac = fopen(pacPath.u8string().c_str(), "wb");

	fseek(fi, 84, SEEK_SET);
	fmdx_entry entry;
	for (size_t i = 0; i < fileCount; i++) {
        LOGBLK
		entry.name[128] = 0;
		fread(entry.name, 1, 128, fi);
		fread(&entry.offset, 4, 1, fi);
		fread(&entry.size, 4, 1, fi);
		fseek(fi, 8, SEEK_CUR);
		size_t offs = ftell(fi);
		
		LOGVER("offset: %8d, size: %8d, name: %s", entry.offset, entry.size, entry.name);

		fseek(fi, entry.offset, SEEK_SET);

		entry.data.resize(entry.size);
		fread(entry.data.data(), 1, entry.size, fi);
		fs::path fullOutPath = rootDirOut;
		fullOutPath /= fs::u8path(entry.name);
		fs::create_directories(fullOutPath.parent_path());
		FILE* fo = fopen(fullOutPath.u8string().c_str(), "wb");
        if(!fo){LOGWAR("errno %d: %s", errno, strerror(errno)); printf("\n"); }
		fwrite(entry.data.data(), 1, entry.size, fo);
		fclose(fo);
		fseek(fi, offs, SEEK_SET);

		fwrite(entry.name, 1, strlen(entry.name), fpac);
		fwrite("\n", 1, 1, fpac);
	}

	fclose(fpac);
    fclose(fi);

	return true;
}

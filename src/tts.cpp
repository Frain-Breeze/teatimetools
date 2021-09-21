#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <vector>
namespace fs = std::filesystem;

#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "logging.hpp"

#define CHARSIZE 16

bool conversation_extract(fs::path fileIn, fs::path texIn, fs::path fileOut){
    FILE* fi = fopen(fileIn.u8string().c_str(), "rb");

    int x = 0, y = 0, channels;
    uint8_t* pix_in = stbi_load(texIn.u8string().c_str(), &x, &y, &channels, 4);

    LOGINF("loaded sheet size: %dx%d", x, y);

    fseek(fi, 0, SEEK_END);
    size_t fsize = ftell(fi);
    int lines = fsize/64;
    fseek(fi, 0, SEEK_SET);

    std::vector<uint8_t> pix_out;
    int end_width = 30 * CHARSIZE;
    int end_height = lines * CHARSIZE;
    pix_out.resize(end_height * end_width * 4);

    for(int i = 0; i < lines; i++){
        fseek(fi, 4, SEEK_CUR);
        uint16_t chars[30];
        fread(chars, 60, 1, fi);
        LOGINF("working on line %d", i+1);
        logging::indent();
        for(int a = 0; a < 30; a++){
            int sheet_x = (chars[a] % (512/CHARSIZE)) * CHARSIZE;
            int sheet_y = (chars[a] / (512/CHARSIZE)) * CHARSIZE;
            int end_x = a*CHARSIZE;
            int end_y = i*CHARSIZE;

            LOGINF("char %2d of line %3d is %4d, taking from %3d,%3d, setting to %3d,%3d", a+1, i+1, chars[a], sheet_x, sheet_y, end_x, end_y);

            for(int o = 0; o < CHARSIZE; o++){
                memcpy(&pix_out[((o + end_y) * (end_width * 4)) + (end_x * 4)],
                       &pix_in[((o + sheet_y) * (512 * 4)) + (4 *sheet_x)], CHARSIZE * 4);
            }
        }
        logging::undent();
    }

    stbi_write_png(fileOut.u8string().c_str(), end_width, end_height, 4, pix_out.data(), end_width * 4);

    STBI_FREE(pix_in);

    return true;
}

bool tts_extract(fs::path fileIn, fs::path dirOut){
    FILE* fi = fopen(fileIn.u8string().c_str(), "rb");
    fseek(fi, 3, SEEK_SET);
    uint8_t things[9];
    fread(things, 8, 1, fi);
    fseek(fi, 3, SEEK_CUR);
    uint8_t entry_count = 0;
    fread(&entry_count, 1, 1, fi);
    fread(&things[8], 1, 1, fi);

    uint32_t toskip = 0;
    for(int i = 0; i < 9; i++){
        toskip += things[i];
    }
    fseek(fi, toskip * 4, SEEK_CUR);

    LOGINF("toskip: %d, entries: %d", toskip, entry_count);
    for(int i = 0; i < entry_count; i++){
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t type = 0;
        fread(&offset, 4, 1, fi);
        fread(&size, 4, 1, fi);
        fread(&type, 4, 1, fi);

        size_t old_offs = ftell(fi);

        fseek(fi, offset, SEEK_SET);
        std::vector<uint8_t> data;
        data.resize(size);
        fread(data.data(), size, 1, fi);
        fs::path out_path = dirOut;

        std::string extension = "";
        std::string typeprint;
        if(type == 0) { typeprint = "GBIX (.uvr)"; extension = ".uvr"; }
        else if(type == 1) { typeprint = "conversation text"; extension = ".bin"; }
        else if(type == 2) { typeprint = "VAG (.vag)"; extension = ".vag"; }
        else { typeprint = "unknown"; extension = ".bin"; }
        LOGINF("entry %3d: offset %-8d  size %-8d  type %d: %s", i+1, offset, size, type, typeprint.c_str());

        out_path /= std::to_string(i+1) + extension;
        FILE* fo = fopen(out_path.u8string().c_str(), "wb");
        fwrite(data.data(), size, 1, fo);
        fclose(fo);

        fseek(fi, old_offs, SEEK_SET);
    }

    return true;
}

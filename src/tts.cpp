#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <array>
#include <tuple>
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

bool tts_repack(fs::path dirIn, fs::path fileOut){

}

//TODO: there's actually not 5 chars, there might be more or less...
static std::array<const char*, 5> chara_names {
    "yui",
    "mio",
    "ritsu",
    "mugi",
    "azusa",
    //...
};

//TODO: figure out other bubble types (if there are any)
static std::array<const char*, 3> bubble_types {
    "normal",
    "thought",
    "scream",
    //...
};

enum class TTSOP : uint8_t {
    SCREEN_ENTER_NAME = 0x02, //no data
    END = 0x03, //no data
    //0x05 len 2?
    //0x07 len 3?
    //0x08 len 5?
    SCREEN_EXPLANATION = 0x0E, //one byte data
    //0x0F len 2?
    SCREEN_MENU = 0x0D, //one byte data
    //0x12 len 1?
    CHARA_SET_POS = 0x13, //4 bytes data
    CHARA_SET_POSE = 0x14, //4 bytes data
    CHARA_SET_FACE = 0x15, //2 bytes data
    CHARA_SET_EMOTION = 0x16, //4 bytes data
    CHARA_MOVE_POS = 0x17, //6 bytes data
    CHARA_LOOKAT_POINT = 0x18, //5 bytes data
    //0x19 len 3?
    CHARA_LOOKAT_CHARA = 0x1A, //4 bytes data
    CHARA_SET_ITEM = 0x1B, //3 bytes data
    OBJ_SET_POS = 0x1C, //4 bytes data
    DOOR_ANIMATION = 0x1D, //one byte data
    OBJ_LOOKAT_POINT = 0x1E, //3 bytes data
    //0x1F len 3?
    DELAY = 0x20, //2 bytes data
    //0x21 len 4?
    //0x22 len 1?
    IMAGE_DISPLAY = 0x23, //2 bytes data
    TEXTBOX_CONTROL = 0x24, //2 bytes data
    //0x25 len 2?
    SCREEN_POPUP = 0x26, //one byte data
    //any more? no clue
};

#define TTSSWC(name, vars, ...) case TTSOP::name: { snprintf(str, 2048, #name " " vars, ##__VA_ARGS__); curr+=std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value; break; }
#define TTSSWS(name, ...) case TTSOP::name: { __VA_ARGS__; break; }

std::string tts_action_extract(const uint8_t& data, size_t data_size){
    std::string ret = "";

    int processed_commands = 0;
    const uint8_t* curr = &data;
    bool ended_gracefully = false;
    bool ended_badly = false;
    char str[2048];
    {
        LOGBLK
        while(curr < (&data + data_size) && !(ended_gracefully || ended_badly)){
            const TTSOP opc = (TTSOP)*curr; curr++;
            switch(opc){
                //TTSSWC(SCREEN_ENTER_NAME
                TTSSWS(END, ended_gracefully = true; LOGVER("END"));
                TTSSWC(SCREEN_EXPLANATION, "h%02x;", curr[0]);
                TTSSWC(SCREEN_MENU, "show=%d;", curr[0]);
                TTSSWC(CHARA_SET_POS, "h%02x, h%02x, h%02x, h%02x;", curr[0], curr[1], curr[2], curr[3]);
                TTSSWC(CHARA_SET_POSE, "char=%s, anim=%d, h%02x, h%02x;", chara_names[curr[0]], curr[1], curr[2], curr[3]);
                TTSSWC(CHARA_SET_FACE, "char=%s, face=%d;", chara_names[curr[0]], curr[1]);
                TTSSWC(CHARA_SET_EMOTION, "char=%s, effect=%d, length=%d%s;", chara_names[curr[0]], curr[1], *((uint16_t*)&curr[2]), "");
                TTSSWC(CHARA_MOVE_POS, "char=%s, anim=%d, xpos=%d, ypos=%d, length=%d%s;", chara_names[curr[0]], curr[1], curr[2], curr[3], *((uint16_t*)&curr[4]), "");
                TTSSWC(CHARA_LOOKAT_POINT, "char=%s, xpos=%d, ypos=%d, length=%d%s;", chara_names[curr[0]], curr[1], curr[2], *((uint16_t*)&curr[3]), "");
                TTSSWC(CHARA_LOOKAT_CHARA, "char=%s, target=%s, length=%d%s;", chara_names[curr[0]], chara_names[curr[1]], *((uint16_t*)&curr[2]), "");
                //TTSSWC(CHARA_SET_ITEM
                TTSSWC(OBJ_SET_POS, "h%02x, h%02x, h%02x, h%02x;", curr[0], curr[1], curr[2], curr[3]);
                TTSSWC(DOOR_ANIMATION, "action=%d;", curr[0]);
                TTSSWC(OBJ_LOOKAT_POINT, "index=%d, xpos=%d, ypos=%d;", curr[0], curr[1], curr[2]);
                TTSSWC(DELAY, "length=%d%s;", *((uint16_t*)&curr[0]), "");
                TTSSWC(IMAGE_DISPLAY, "bg=%d, action=%d;", curr[0], curr[1]);
                TTSSWC(TEXTBOX_CONTROL, "h%02x, type=%s;", curr[0], bubble_types[curr[1]]);
                TTSSWC(SCREEN_POPUP, "popup=%d;", curr[0]);
                default: {
                    LOGERR("unknown opcode (0x%02x) found!", (uint8_t)opc);
                    ended_badly = true;
                    break;
                }
            }

            processed_commands++;

            if(!ended_badly && !ended_gracefully){
                LOGVER("%s", str);
                ret += str;
                ret += "\n";
            }
        }
    }


    LOGINF("processed %d commands", processed_commands);

    return ret;
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

    uint32_t first_offset = 0;
    LOGINF("toskip: %d, entries: %d", toskip, entry_count);
    for(int i = 0; i < entry_count; i++){
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t type = 0;
        fread(&offset, 4, 1, fi);
        fread(&size, 4, 1, fi);
        fread(&type, 4, 1, fi);

        if(i == 0){ first_offset = offset; }

        size_t old_offs = ftell(fi);

        fseek(fi, offset, SEEK_SET);
        std::vector<uint8_t> data(size);
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

    { //convert bytecode to text format
        LOGBLK
        fseek(fi, 8, SEEK_CUR);
        uint32_t curr_offset = ftell(fi);
        uint32_t act_size = curr_offset - first_offset;

        std::vector<uint8_t> act_data(act_size);
        fread(act_data.data(), act_size, 1, fi);

        std::string act_ret = tts_action_extract(*act_data.data(), act_data.size());

        fs::path out_path = dirOut;
        out_path /= "script.txt";
        FILE* fso = fopen(out_path.u8string().c_str(), "w");

        fwrite(act_ret.c_str(), strlen(act_ret.c_str()), 1, fso);
        fclose(fso);
    }



    return true;
}

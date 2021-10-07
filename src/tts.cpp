#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <array>
#include <tuple>
#include <map>
#include <memory>
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

class TYP_root {
public:
    //TYP_root(){};
    virtual int byteSize() = 0;
    virtual uint16_t toByte(std::string text) = 0;
    virtual std::string toText(uint16_t data) = 0;
};

class TYP_unk : public TYP_root {
public:
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text, 0, 16); }
    std::string toText(uint16_t data) override { char buf[3]; snprintf(buf, 3, "%02x", data); return buf; }
};
class TYP_u8 : public TYP_root {
public:
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_u16 : public TYP_root {
public:
    int byteSize() override { return 2; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_char : public TYP_root {
public:
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override {
        for(size_t i = 0; i < chara_names.size(); i++)
            if(text == chara_names[i])
                return i;

        LOGERR("not implemented: scanning from int (or name is invalid)");
        return -1;
    }

    std::string toText(uint16_t data) override {
        if(data < chara_names.size()){
            return chara_names[data];
        }
        return std::to_string(data);
    }
};
class TYP_obj : public TYP_root {
    //TODO: implement actual obj names
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_face : public TYP_root {
    //TODO: implement actual face names
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_pose : public TYP_root {
    //TODO: implement actual face names
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_bg : public TYP_root {
    //TODO: implement actual bg names
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_bubble : public TYP_root {
public:
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override {
        for(size_t i = 0; i < bubble_types.size(); i++)
            if(text == bubble_types[i])
                return i;

        LOGERR("not implemented: scanning from int (or name is invalid)");
        return -1;
    }

    std::string toText(uint16_t data) override {
        if(data < bubble_types.size()){
            return bubble_types[data];
        }
        return std::to_string(data);
    }
};
class TYP_eff : public TYP_root {
    //TODO: implement actual eff names
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};
class TYP_popup : public TYP_root {
    //TODO: implement actual popup names (...if they have names?)
    int byteSize() override { return 1; }
    uint16_t toByte(std::string text) override { return (uint16_t)std::stoi(text); }
    std::string toText(uint16_t data) override { return std::to_string(data); }
};

struct TTSCOM {
    TTSOP op;
    std::string name;
    const int data_length = -1;
    struct TYPE {
        std::string name;
        TYP_root* type = nullptr;
    };
    std::vector<TYPE> types; //keeps text name + converter, order is important (defines position in command)
};

TYP_unk t_unk;
TYP_u8 t_u8;
TYP_u16 t_u16;
TYP_char t_char;
TYP_obj t_obj;
TYP_face t_face;
TYP_pose t_pose;
TYP_bg t_bg;
TYP_bubble t_bubble;
TYP_eff t_eff;
TYP_popup t_popup;

#define A(name) TTSOP::name, #name
static std::vector<TTSCOM> comms {
    { A(SCREEN_ENTER_NAME), 0, {}},
    { A(END), 0, {} },
    { A(SCREEN_EXPLANATION), 1, {{"0h", &t_unk}} },
    { A(SCREEN_MENU), 1, {{"show", &t_u8}} },
    { A(CHARA_SET_POS), 4, {{"0h", &t_unk}, {"1h", &t_unk}, {"2h", &t_unk}, {"3h", &t_unk}} },
    { A(CHARA_SET_POSE), 4, {{"char", &t_char}, {"pose", &t_pose}, {"2h", &t_unk}, {"3h", &t_unk}} },
    { A(CHARA_SET_FACE), 2, {{"char", &t_char}, {"face", &t_face}} },
    { A(CHARA_SET_EMOTION), 4, {{"char", &t_char}, {"eff", &t_eff}, {"length", &t_u16}} },
    { A(CHARA_MOVE_POS), 6, {{"char", &t_char}, {"pose", &t_pose}, {"xpos", &t_u8}, {"ypos", &t_u8}, {"length", &t_u16}} },
    { A(CHARA_LOOKAT_POINT), 5, {{"char", &t_char}, {"xpos", &t_u8}, {"ypos", &t_u8}, {"length", &t_u16}} },

    { A(CHARA_LOOKAT_CHARA), 4, {{"char", &t_char}, {"lookat", &t_char}, {"length", &t_u16}} },
    { A(CHARA_SET_ITEM), 3, {{"char", &t_char}, {"hidden", &t_u8}, {"obj", &t_obj}} },
    { A(OBJ_SET_POS), 4, {{"0h", &t_unk}, {"1h", &t_unk}, {"2h", &t_unk}, {"3h", &t_unk}} },
    { A(DOOR_ANIMATION), 1, {{"0h", &t_unk}} },
    { A(OBJ_LOOKAT_POINT), 3, {{"obj", &t_obj}, {"xpos", &t_u8}, {"ypos", &t_u8}} },

    { A(DELAY), 2, {{"length", &t_u16}} },

    { A(IMAGE_DISPLAY), 2, {{"bg", &t_bg}, {"1h", &t_unk}} },
    { A(TEXTBOX_CONTROL), 2, {{"0h", &t_unk}, {"bubble", &t_bubble}} },

    { A(SCREEN_POPUP), 1, {{"popup", &t_popup}} },
};


#define TTSSWC(name, vars, ...) case TTSOP::name: { snprintf(str, 2048, #name " " vars, ##__VA_ARGS__); curr+=std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value; break; }
#define TTSSWS(name, ...) case TTSOP::name: { __VA_ARGS__; break; }

bool tts_action_compile(const std::string& input, std::vector<uint8_t>& data_out){
    size_t curr = 0;
    while(input.size() > curr) {
        size_t end_of_line = input.find("\n", curr);
        size_t first_space = input.find(" ", curr);
        if(first_space < end_of_line){
            std::string comname = input.substr(curr, first_space - curr);
            TTSCOM* com = nullptr;
            for(auto& c : comms){ if(c.name == comname){ com = &c; } }
            if(com){
                LOGVER("found %d with name %s and size %d", com->op, com->name.c_str(), com->data_length);
                data_out.push_back((uint8_t)com->op);
                data_out.resize(data_out.size() + com->data_length);

                std::vector<std::string> parts;
                size_t cpos = first_space+1;
                size_t old_cpos = cpos;
                while((cpos = input.find_first_of(" ;", old_cpos)) != std::string::npos
                    && (cpos < end_of_line)
                    && (parts.size() < com->types.size())){

                    parts.push_back(input.substr(old_cpos, cpos - old_cpos));
                    cpos++;
                    old_cpos = cpos;
                }

                std::vector<bool> handled(com->types.size()); //TODO: check if guaranteed zero initialization, and also use this to see if all variables were found
                for(int i = 0; i < parts.size(); i++){
                    std::string varname = parts[i].substr(0, parts[i].find("="));
                    std::string value = parts[i].substr(parts[i].find("=") + 1, parts[i].find(",") - parts[i].find("=") + 1);
                    size_t offs = 0;
                    for(int a = 0; a < com->types.size(); a++){
                        if(varname == com->types[a].name){
                            handled[a] = true;
                            uint16_t type_data = com->types[a].type->toByte(value);
                            size_t offs_into_data = (data_out.size() - com->data_length) + offs;
                            if(com->types[a].type->byteSize() == 1){
                                data_out[offs_into_data] = type_data & 0xFF;
                                break;
                            }else if(com->types[a].type->byteSize() == 2){
                                data_out[offs_into_data] = type_data & 0xFF;
                                data_out[offs_into_data+1] = (type_data >> 8) & 0xFF;
                                break;
                            }else{
                                LOGERR("not implemented, very bad");
                                return false;
                            }
                        }
                        offs += com->types[a].type->byteSize();
                    }
                }
            }else{
                LOGERR("fix this"); //TODO: error stuff
            }
        }
        curr = end_of_line+1;
    }

    return true;
}

std::string tts_action_decompile(const uint8_t& data, size_t data_size){
    std::string ret = "";

    int processed_commands = 0;
    const uint8_t* curr = &data;
    {
        std::string str;
        LOGBLK
        while(curr < (&data + data_size)){
            TTSCOM* com = nullptr;
            for(auto& a : comms){ if(a.op == (TTSOP)curr[0]) { com = &a; break; }}
            if(com){
                str = com->name;
                curr++;
                for(const auto &p : com->types){
                    str += " " + p.name + "=" + p.type->toText(curr[0]);
                    curr += p.type->byteSize();
                }
                LOGVER("%s", str.c_str());
                str += ";\n";
                processed_commands++;
            }
            else{
                LOGERR("unknown command %d (0x%02x) found, aborting decompilation.", curr[0], curr[0]);
                return "";
            }
            ret += str;
        }
    }

    LOGINF("processed %d commands", processed_commands);



    return ret;
}

bool tts_repack(fs::path dirIn, fs::path fileOut){
    std::vector<uint8_t> compiled_script_data;
    { //script data
        LOGBLK
        fs::path scriptPath = dirIn;
        scriptPath /= "script.txt";
        FILE* fi = fopen(scriptPath.u8string().c_str(), "rb");
        if(!fi){
            LOGINF("no script.txt found, so event will immediately end");
            compiled_script_data.push_back((uint8_t)TTSOP::END);
        }else{
            fseek(fi, 0, SEEK_END);
            std::string data;
            data.resize(ftell(fi));
            fseek(fi, 0, SEEK_SET);
            fread(data.data(), data.size(), 1, fi);

            tts_action_compile(data, compiled_script_data);
        }
    }

    struct ENTRY{
        uint32_t size = -1;
        uint32_t offset = -1;
        enum : uint32_t{
            TUVR = 0,
            TTEXT = 1,
            TVAG = 2,
        } type;
    };

    //get entry count
    std::vector<ENTRY> entries;
    for(const auto& p : fs::directory_iterator(dirIn)){
        int curr_entry = -1;
        sscanf(p.path().filename().u8string().c_str(), "%d.txt", &curr_entry);
        if(curr_entry != -1) {
            size_t fsize = fs::file_size(p.path());
            if(curr_entry + 1 > entries.size()){ entries.resize(curr_entry + 1); }
            entries[curr_entry].size = fsize;
        }
    }

    LOGVER("found %d entries", entries.size());





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
        fs::create_directories(out_path.parent_path());
        FILE* fo = fopen(out_path.u8string().c_str(), "wb");
        fwrite(data.data(), size, 1, fo);
        fclose(fo);

        fseek(fi, old_offs, SEEK_SET);
    }

    { //convert bytecode to text format
        LOGBLK
        fseek(fi, 8, SEEK_CUR);
        uint32_t curr_offset = ftell(fi);
        uint32_t act_size = first_offset - curr_offset;

        std::vector<uint8_t> act_data(act_size);
        fread(act_data.data(), act_size, 1, fi);

        std::string act_ret = tts_action_decompile(*act_data.data(), act_data.size());

        fs::path out_path = dirOut;
        out_path /= "script.txt";
        FILE* fso = fopen(out_path.u8string().c_str(), "w");

        fwrite(act_ret.c_str(), strlen(act_ret.c_str()), 1, fso);
        fclose(fso);
    }



    return true;
}

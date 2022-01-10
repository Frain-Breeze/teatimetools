#pragma once

#include <array>
#include <vector>
#include <stdint.h>
#include <string>

#include "logging.hpp"

static std::array<const char*, 8> chara_names {
    "yui",
    "mio",
    "ritsu",
    "mugi",
    "azusa",
    "sawako",
	"nodoka",
	"ui"
};

//TODO: figure out other bubble types (if there are any)
static std::array<const char*, 3> bubble_types {
    "normal",
    "thought",
    "scream",
    //...
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


        int scanint = -1;
        sscanf(text.c_str(), "%d", &scanint);
        if(scanint == -1) {
            LOGERR("couldn't parse \"%s\" to int. maybe you made a typo?", text.c_str());
            return -1;
        }
        return scanint;
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
class TYP_img : public TYP_root {
    //TODO: implement actual img names
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

TYP_unk t_unk;
TYP_u8 t_u8;
TYP_u16 t_u16;
TYP_char t_char;
TYP_obj t_obj;
TYP_face t_face;
TYP_pose t_pose;
TYP_bg t_bg;
TYP_img t_img;
TYP_bubble t_bubble;
TYP_eff t_eff;
TYP_popup t_popup;

enum class TTSOP : uint8_t {
    SCREEN_ENTER_NAME = 0x02, //no data
    END = 0x03, //no data
    //0x05 len 2?
    UNK_07 = 0x07, //3 bytes data
    UNK_08 = 0x08, //2 bytes data, something to do with scene switches?
    UNK_09 = 0x09, //2 bytes data
    SCREEN_EXPLANATION = 0x0E, //one byte data
    CHARA_SET_INSTRUMENT = 0x0F, //two byte data
    SCREEN_MENU = 0x0D, //one byte data
    IMAGE_BACKGROUND = 0x11, //two bytes data
    SCREEN_VISIBLE = 0x12, //one byte data
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
    OBJ_MOVE_POS = 0x1F, //3 bytes data
    DELAY = 0x20, //2 bytes data
    UNK_21 = 0x21, //something with set BGM, 4 bytes data
    SFX_PLAY = 0x22, //1 byte data
    IMAGE_DISPLAY = 0x23, //2 bytes data
    TEXTBOX_CONTROL = 0x24, //2 bytes data
    //0x25 len 2?
    SCREEN_POPUP = 0x26, //one byte data
    //any more? no clue
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

#define A(name) TTSOP::name, #name
static std::vector<TTSCOM> comms {
    { A(SCREEN_ENTER_NAME), 0, {}},
    { A(END), 0, {} },

    { A(UNK_07), 3, {{"0h", &t_unk}, {"1h", &t_unk}, {"2h", &t_unk}} },
    { A(UNK_08), 2, {{"0h", &t_unk}, {"1h", &t_unk}} },
    { A(UNK_09), 2, {{"0h", &t_unk}, {"1h", &t_unk}} },

    { A(SCREEN_EXPLANATION), 1, {{"0h", &t_unk}} },
    { A(CHARA_SET_INSTRUMENT), 2, {{"char", &t_char}, {"on", &t_u8}} },
    { A(SCREEN_MENU), 1, {{"show", &t_u8}} },
    { A(IMAGE_BACKGROUND), 2, {{"bg", &t_bg}, {"subbg", &t_u8}} },
    { A(SCREEN_VISIBLE), 1, {{"visible", &t_u8}} },
    { A(CHARA_SET_POS), 4, {{"char", &t_char}, {"1h", &t_unk}, {"2h", &t_unk}, {"3h", &t_unk}} },
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
    { A(OBJ_MOVE_POS), 3, {{"index", &t_u8}, {"xpos", &t_u8}, {"ypos", &t_u8}} },
    { A(DELAY), 2, {{"length", &t_u16}} },
    { A(UNK_21), 4, {{"0h", &t_unk}, {"1h", &t_unk}, {"2h", &t_unk}, {"3h", &t_unk}} },
    { A(SFX_PLAY), 1, {{"track", &t_u8}} },

    { A(IMAGE_DISPLAY), 2, {{"img", &t_img}, {"1h", &t_unk}} },
    { A(TEXTBOX_CONTROL), 2, {{"0h", &t_unk}, {"bubble", &t_bubble}} },

    { A(SCREEN_POPUP), 1, {{"popup", &t_popup}} },
};

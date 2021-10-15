#include "cpk.hpp"

#include <string.h>

// https://github.com/esperknight/CriPakTools/blob/0dbd2229f7c3b519c67d65e89055fd4e610d6dca/CriPakTools/CPK.cs#L577

void decryptUTF(std::vector<uint8_t>& data) {
    uint32_t m = 0x0000655f;
    uint32_t t = 0x00004115;

    uint8_t d;

    for(size_t i = 0; i < data.size(); i++) {
        d = data[i];
        d = d ^ (m & 0xFF);
        data[i] = d;
        m *= t;
    }
}

void readUTF(Tea::File& file, std::vector<uint8_t>& out_data) {
    uint32_t unk1; file.read(unk1);
    uint64_t utf_size; file.read(utf_size);
    out_data.resize(utf_size);
    file.read(out_data.data(), utf_size);
    if(out_data[0] != 0x40 || out_data[1] != 0x55 || out_data[2] != 0x54 || out_data[3] != 0x46) { //@UTF
        decryptUTF(out_data);
    }
}

bool CPK::open(Tea::File& file) {
    this->close();

    _file = &file;

    char magic[5] = "\0\0\0\0";
    _file->read((uint8_t*)magic, 4);
    if(strncmp(magic, "CPK ", 4)) { return false; }

    std::vector<uint8_t> CPK_packet;
    readUTF(*_file, CPK_packet);

    filetable.resize(1);
    Entry ent;
    ent.name = "CPK_HDR";
    ent.type = "CPK";
    ent.offset = _file->tell();

    //TODO: continue
}

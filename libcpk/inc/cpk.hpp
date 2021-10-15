#pragma once

#include <teaio_file.hpp>
#include <vector>
#include <string>

class CPK {
public:
    bool open(Tea::File& file);
    bool close();

    struct Entry {
        std::string name;
        bool encrypted;
        std::string type;
        size_t offset;
        size_t size;
    };
    std::vector<Entry> filetable;

private:
    Tea::File* _file = nullptr;
};

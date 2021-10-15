#include "teaio_file.hpp"

#include <filesystem>
namespace fs = std::filesystem;

template<typename T>
bool Tea::File::read(T& type, Endian endian) {
    return this->read_endian(reinterpret_cast<uint8_t*>(&type), sizeof(T), endian);
}

template<typename T>
T Tea::File::read(Endian endian) {
    T type;
    this->read_endian(reinterpret_cast<uint8_t*>(&type), sizeof(T), endian);
    return type;
}




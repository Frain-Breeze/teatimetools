#include <teaio_file.hpp>

#include <stdint.h>
#include <stdio.h>

bool Tea::FileDisk::open(const char* const path, Tea::Access flags, Tea::Endian endian) {
    const char* rwflags;
    if(flags & Tea::Access_read) {
        if(flags & Tea::Access_write) { rwflags = "wb+"; }
        else { rwflags = "rb"; }
    }
    else {
        if(flags & Tea::Access_write) { rwflags = "wb"; }
        else { return false; }
    }

    _endian = endian;

    this->close();
    _fp = fopen(path, rwflags); //TODO: path error stuff

    if(!_fp) { return false; }

    fseek(_fp, 0, SEEK_END);
    _size = ftell(_fp);
    fseek(_fp, 0, SEEK_SET);

    return true;
}

bool Tea::FileDisk::close() {
    if(_fp)
        fclose(_fp);
    else
        return false;
    _fp = nullptr;
    return true;
}

bool Tea::FileDisk::read(uint8_t* data, size_t size) {
    if(!_fp)
        return false;

    size_t ret = fread((void*)data, size, 1, _fp);
    //TODO: error

    _offset += size;

    return ret;
}

bool Tea::FileDisk::read_endian(uint8_t* data, size_t size, Endian endian) {
    if(!_fp)
        return false;

    size_t ret = fread((void*)data, size, 1, _fp);
    //TODO: error

    //swap to endian
    if(endian == Tea::Endian_current) { endian = _endian; }

    //assume we're on little endian (x86)
    if(endian == Tea::Endian_big) {
        for(int i = 0; i < (size / 2); i++) {
            uint8_t tmp = data[size - i];
            data[size - i] = data[i];
            data[i] = tmp;
        }
    }

    _offset += size;

    return ret;
}

bool Tea::FileDisk::skip(int64_t length) {
    return this->seek(length, Tea::Seek_current);
}

bool Tea::FileDisk::seek(int64_t pos, Tea::Seek mode) {
    if(!_fp)
        return false;

    int nmode;
    if(mode == Tea::Seek_current) { nmode = SEEK_CUR; _offset += pos; }
    else if(mode == Tea::Seek_end) { nmode = SEEK_END; _offset = _size; }
    else if(mode == Tea::Seek_start) { nmode = SEEK_SET; _offset = pos; }
    else { return false; }

    fseek(_fp, pos, nmode); //TODO: error stuff here

    return true;
}

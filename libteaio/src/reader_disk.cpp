#include <teaio_file.hpp>

#include <stdint.h>
#include <stdio.h>
#include <algorithm>

#include <logging.hpp>


bool Tea::FileDisk::open(const char* const path, Tea::Access flags, Tea::Endian endian) {
    _access_flags = flags;
    
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

bool Tea::FileDisk::write_file(Tea::File& file, size_t size) {
    if(!_fp)
        return false;
    
    size_t to_write = std::min(size, file.size() - file.tell());
    
    uint8_t buf[4096];
    for(size_t i = 0; i + 4096 < to_write; i += 4096) {
        file.read(buf, 4096);
        this->write(buf, 4096);
    }
    
    //read remaining part of buffer
    file.read(buf, to_write % 4096);
    this->write(buf, to_write % 4096);
    
    return true;
}

bool Tea::FileDisk::write(uint8_t* data, size_t size) {
    if(!_fp)
        return false;
    
    if(!(_access_flags & Tea::Access_write))
        return false;
    
    if(_offset + size > _size)
        _size = _offset + size;
    
    _offset += size;
    
    return fwrite(data, size, 1, _fp);
}

bool Tea::FileDisk::write_endian(uint8_t* data, size_t size, Endian endian) {
    if(!_fp)
        return false;
    
    if(!(_access_flags & Tea::Access_write))
        return false;
    
    if(_offset + size > _size)
        _size = _offset + size;
    
    _offset += size;
    
    //HACK: very inefficient, add in buffer system maybe?
    //swap to endian
    if(endian == Tea::Endian::current) { endian = _endian; }

    bool writing_issue = false;
    
    //assume we're on little endian (x86)
    if(endian == Tea::Endian::big) {
        for(int i = size - 1; i >= 0; i--) {
            writing_issue |= !fwrite(&data[i], 1, 1, _fp);
        }
    }
    else {
        writing_issue |= !fwrite(data, size, 1, _fp);
    }
    
    return !writing_issue;
}

bool Tea::FileDisk::read_endian(uint8_t* data, size_t size, Endian endian) {
    if(!_fp)
        return false;

    size_t ret = fread((void*)data, size, 1, _fp);
    //TODO: error

    //swap to endian
    if(endian == Tea::Endian::current) { endian = _endian; }

    //assume we're on little endian (x86)
    if(endian == Tea::Endian::big) {
        for(int low = 0, high = size - 1; low < high; low++, high--) {
            uint8_t tmp = data[low];
            data[low] = data[high];
            data[high] = tmp;
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

Tea::FileDisk::~FileDisk() {
    this->close();
}

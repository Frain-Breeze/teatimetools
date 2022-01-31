#include <teaio_file.hpp>

#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <algorithm>

#include <logging.hpp>

bool Tea::FileMemory::open(uint8_t* data, size_t data_size, Endian endian) {
    this->close();
    if(!data)
        return false;

    _buffer = data;
    _size = data_size;
    _endian = endian;
    _owner = false;

    return true;
}

bool Tea::FileMemory::open_owned(Endian endian) {
    //TODO: handle re-opening (aka if buffer is already active)
    _buffer = (uint8_t*)malloc(0);
    _owner = true;
    if(!_buffer)
        return false;
    return true;
}

bool Tea::FileMemory::open_owned(uint8_t* data, size_t data_size, Endian endian) {
    this->open(data, data_size, endian);
    _owner = true;

    return true;
}

bool Tea::FileMemory::read(uint8_t* data, size_t size) {
    if(!_buffer)
        return false;

    if(_offset + size > _size) { return false; }
    memcpy(data, &_buffer[_offset], size);
    _offset += size;

    return true;
}

bool Tea::FileMemory::write_file(Tea::File& file, size_t size) {
    if(!_buffer)
        return false;
    
    size_t to_write = std::min(size, file.size() - file.tell());
    
    if(_offset + to_write > _size) {
        if(!_owner)
            return false;
        
        _buffer = (uint8_t*)realloc(_buffer, _offset + to_write);
        _size = _offset + to_write;
    }
    
    //check if realloc went okay
    if(!_buffer)
        return false;
    
    bool ret = file.read(_buffer + _offset, to_write);
    return ret;
}

bool Tea::FileMemory::write_endian(uint8_t* data, size_t size, Endian endian) {
    if(!_buffer)
        return false;
    
    if(_offset + size > _size) {
        if(!_owner)
            return false;
        
        _buffer = (uint8_t*)realloc(_buffer, _offset + size);
        _size = _offset + size;
    }
    
    //check if realloc went okay
    if(!_buffer)
        return false;
    
    if(memcpy(_buffer + _offset, data, size) != _buffer + _offset)
        return false;
    
    //swap to endian
    if(endian == Tea::Endian::current) { endian = _endian; }

    //assume we're on little endian (x86)
    if(endian == Tea::Endian::big) {
        for(int low = 0, high = size - 1; low < high; low++, high--) {
            uint8_t tmp = _buffer[_offset + low];
            _buffer[_offset + low] = _buffer[_offset + high];
            _buffer[_offset + high] = tmp;
        }
    }
    
    _offset += size;
    
    return true;
}

bool Tea::FileMemory::write(uint8_t* data, size_t size) {    
    if(!_buffer)
        return false;
    
    if(_offset + size > _size) {
        if(!_owner)
            return false;
        
        _buffer = (uint8_t*)realloc(_buffer, _offset + size);
        _size = _offset + size;
    }
    
    //check if realloc went okay
    if(!_buffer)
        return false;
    
    if(memcpy(_buffer + _offset, data, size) != _buffer + _offset)
        return false;
    
    _offset += size;
    
    return true;
}

bool Tea::FileMemory::read_endian(uint8_t* data, size_t size, Endian endian) {
    if(!_buffer)
        return false;

    if(_offset + size > _size) { return false; }

    memcpy(data, &_buffer[_offset], size);

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

    return true;
}

bool Tea::FileMemory::seek(int64_t pos, Tea::Seek mode) {
    if(!_buffer)
        return false;

    if(mode == Tea::Seek_current) {
        if(_offset + pos < 0) { return false; };
        if(_offset + pos > _size) {
            if(_owner) { _buffer = (uint8_t*)realloc((void*)_buffer, _offset + pos); _size = _offset + pos; }
            else { return false; }
        }
        _offset += pos;
    }
    else if(mode == Tea::Seek_end) { _offset = _size; }
    else if(mode == Tea::Seek_start) {
        if(pos < 0) { return false; };
        if(pos > _size) {
            if(_owner) { _buffer = (uint8_t*)realloc((void*)_buffer, pos); _size = pos; }
            else { return false; }
        }
        _offset = pos;
    }
    else { return false; }

    return true;
}

bool Tea::FileMemory::skip(int64_t length) {
    return this->seek(length, Tea::Seek_current);
}

bool Tea::FileMemory::close() {
    if(_owner)
        free(_buffer);
    return true;
}

Tea::FileMemory::~FileMemory() {
    this->close();
}

uint8_t* Tea::FileMemory::unsafe_get_buffer() { return _buffer; }

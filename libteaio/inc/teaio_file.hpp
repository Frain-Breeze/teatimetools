#pragma once

#include <stdint.h>
#include <stdio.h>

#include <logging.hpp>

namespace Tea {
    typedef int Endian;
    static const Endian Endian_current = 0; //use endian that the file is currently set to
    static const Endian Endian_little = 1;
    static const Endian Endian_big = 2;

    typedef int Access;
    static const Access Access_read = 1;
    static const Access Access_write = 2;

    typedef int Seek;
    static const Seek Seek_start = 0;
    static const Seek Seek_current = 1;
    static const Seek Seek_end = 2;

    class File {
    public:
        //virtual bool open(const char* const path, Access flags, Endian endian = Endian_little) = 0;
        virtual bool close() = 0;

        virtual ~File();

        virtual size_t tell() { return _offset; }
        virtual bool skip(int64_t length) = 0;
        virtual bool seek(int64_t pos, Seek mode = Seek_start) = 0;
        virtual size_t size() { return _size; }

        virtual bool read(uint8_t* data, size_t size) = 0;
        virtual bool read_endian(uint8_t* data, size_t size, Endian endian = Endian_current) = 0;
        virtual bool write(uint8_t* data, size_t size) = 0;

        //sadly, these have to be implemented in the header, because C++ is funny like that
        template<typename T>
        bool read(T& type, Endian endian = Endian_current) {
            return this->read_endian(reinterpret_cast<uint8_t*>(&type), sizeof(T), endian);
        }

        template<typename T>
        T read(Endian endian = Endian_current) {
            T type;
            T* ttype = &type; //for reasons that are beyond me, using &type directly in the reinterpret_cast generates a segfault, but this doesn't.
            this->read_endian(reinterpret_cast<uint8_t*>(ttype), sizeof(T), endian);
            return type;
        }

        Endian endian() { return _endian; }
        Endian endian(Endian new_endian) { _endian = new_endian; return _endian; }

    protected:
        size_t _offset = 0;
        size_t _size = -1;
        Endian _endian = Endian_little;
    };

    class FileDisk : public File {
    public:
        ~FileDisk();

        bool open(const char* const path, Access flags, Endian endian = Endian_little);
        bool close();

        bool skip(int64_t length);
        bool seek(int64_t pos, Seek mode);

        bool read(uint8_t* data, size_t size);
        bool read_endian(uint8_t* data, size_t size, Endian endian);
        bool write(uint8_t* data, size_t size);

    private:
        FILE* _fp = nullptr;
    };

    class FileMemory : public File {
    public:
        ~FileMemory();

        bool open(uint8_t* data, size_t data_size, Endian endian = Endian_little);
        bool open_owned(uint8_t* data, size_t data_size, Endian endian = Endian_little); //transfer ownership to class (will enable resizing support)
        bool close();

        bool skip(int64_t length);
        bool seek(int64_t pos, Seek mode);

        bool read(uint8_t* data, size_t size);
        bool read_endian(uint8_t* data, size_t size, Endian endian);
        bool write(uint8_t* data, size_t size);

    private:
        uint8_t* _buffer = nullptr;
        bool _owner = false;
    };
}





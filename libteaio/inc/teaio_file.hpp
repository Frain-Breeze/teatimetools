#pragma once

#include <stdint.h>
#include <stdio.h>

#include <logging.hpp>

namespace Tea {
	//typedef int Endian;
	//static const Endian Endian_current = 0; //use endian that the file is currently set to
	//static const Endian Endian_little = 1;
	//static const Endian Endian_big = 2;
	
	enum class Endian {
		current = 0,
		little = 1,
		big = 2,
		inherit = 3,
	};
	
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
		virtual bool read_endian(uint8_t* data, size_t size, Endian endian = Endian::current) = 0;
		virtual bool write(uint8_t* data, size_t size) = 0;
		virtual bool write_endian(uint8_t* data, size_t size, Endian endian = Endian::current) = 0;
		virtual bool write_file(Tea::File& file, size_t size = -1) = 0;
		
		//also writes delimiter
		bool write_c_string(const char* data, size_t max_length = -1) {
			size_t str_len = 0;
			while(data[str_len] != '\0' && str_len < max_length) {
				str_len++;
			}
			
			return this->write((uint8_t*)data, str_len + 1); //include delimiter
		}
		
		//sadly, these have to be implemented in the header, because C++ is funny like that
		template<typename T>
		bool read(T& type, Endian endian = Endian::current) {
			return read_endian(reinterpret_cast<uint8_t*>(&type), sizeof(T), endian);
		}
		
		template<typename T>
		T read(Endian endian = Endian::current) {
			T type;
			T* ttype = &type; //for reasons that are beyond me, using &type directly in the reinterpret_cast generates a segfault, but this doesn't.
			this->read_endian(reinterpret_cast<uint8_t*>(ttype), sizeof(T), endian);
			return type;
		}
		
		template<typename T>
		bool write(T type, Endian endian = Endian::current) {
			return this->write_endian((uint8_t*)&type, sizeof(T), endian);
		}
		
		Endian endian() { return _endian; }
		Endian endian(Endian new_endian) { _endian = new_endian; return _endian; }
		
	protected:
		size_t _offset = 0;
		size_t _size = 0;
		Endian _endian = Endian::little;
	};
	
	class FileDisk : public File {
	public:
		~FileDisk();
		
		bool open(const char* const path, Access flags, Endian endian = Endian::little);
		bool close();
		
		bool skip(int64_t length);
		bool seek(int64_t pos, Seek mode);
		
		bool read(uint8_t* data, size_t size);
		bool read_endian(uint8_t* data, size_t size, Endian endian);
		bool write(uint8_t* data, size_t size);
		bool write_endian(uint8_t* data, size_t size, Endian endian = Endian::current);
		bool write_file(Tea::File& file, size_t size);
		
	private:
		FILE* _fp = nullptr;
		Access _access_flags;
	};
	
	class FileMemory : public File {
	public:
		FileMemory(uint8_t& data, size_t data_size, Endian endian = Endian::little) { this->open(&data, data_size, endian); }
		FileMemory() {}
		~FileMemory();
		
		bool open(uint8_t* data, size_t data_size, Endian endian = Endian::little);
		bool open_owned(Endian endian = Endian::little); //allocate the buffer ourselves
		bool open_owned(uint8_t* data, size_t data_size, Endian endian = Endian::little); //transfer ownership to class (will enable resizing support)
		bool close();
		
		bool skip(int64_t length);
		bool seek(int64_t pos, Seek mode = Seek_start);
		
		bool read(uint8_t* data, size_t size);
		bool read_endian(uint8_t* data, size_t size, Endian endian);
		bool write(uint8_t* data, size_t size);
		bool write_endian(uint8_t* data, size_t size, Endian endian = Endian::current);
		bool write_file(Tea::File& file, size_t size);
		
		uint8_t* unsafe_get_buffer();
		
	private:
		uint8_t* _buffer = nullptr;
		bool _owner = false;
	};
	
	//for presenting a subsection of a file to a function without worrying about offsets, endian, etc.
	class FileSection : public File {
	public:
		FileSection(File& new_file, size_t section_offset, size_t section_length, Endian endian = Endian::inherit);
		FileSection() {}
		~FileSection() {};
		
		bool open(File& new_file, size_t section_offset, size_t section_length, Endian endian = Endian::inherit);
		bool close(); //only closes section: the underlying file will stay intact
		
		bool skip(int64_t length);
		bool seek(int64_t pos, Seek mode = Seek_start);
		
		bool read(uint8_t* data, size_t size);
		bool read_endian(uint8_t* data, size_t size, Endian endian);
		bool write(uint8_t* data, size_t size);
		bool write_endian(uint8_t* data, size_t size, Endian endian = Endian::inherit);
		bool write_file(Tea::File& file, size_t size);
	private:
		File* _file = nullptr;
		size_t _sect_offset = -1;
	protected:
		Endian _endian = Endian::inherit;
	};
}





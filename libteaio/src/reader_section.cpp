#include <teaio_file.hpp>

#include <stdint.h>
#include <stdio.h>

Tea::FileSection::FileSection(File& new_file, size_t section_offset, size_t section_length, Endian endian) {
	this->open(new_file, section_offset, section_length, endian);
}

bool Tea::FileSection::open(File& new_file, size_t section_offset, size_t section_length, Endian endian) {
	if(section_length + section_offset > new_file.size()) {
		_file = nullptr;
		return false;
	}
	
	_file = &new_file;
	_size = section_length;
	_sect_offset = section_offset;
	
	if(endian == Endian::inherit)
		_endian = _file->endian();
	else
		_endian = endian;
	
	return true;
}

bool Tea::FileSection::close() {
	bool ret = (_file != nullptr);
	_file = nullptr;
	return ret;
}

bool Tea::FileSection::read(uint8_t* data, size_t size) {
	return this->read_endian(data, size, Endian::current);
}

bool Tea::FileSection::read_endian(uint8_t* data, size_t size, Endian endian) {
	if(_file == nullptr) { return false; }
	if(_offset + size > _size) { return false; }
	
	size_t old_offset = _file->tell();
	_file->seek(_sect_offset + _offset);
	
	bool ret =  _file->read_endian(data, size, endian);
	
	_file->seek(old_offset);
	return ret;
}

bool Tea::FileSection::write(uint8_t* data, size_t size) {
	return this->write_endian(data, size);
}

bool Tea::FileSection::write_endian(uint8_t* data, size_t size, Endian endian) {
	if(_file == nullptr) { return false; }
	if(_offset + size > _size) { return false; } //resizing section not (yet?) allowed
	
	size_t old_offset = _file->tell();
	_file->seek(_sect_offset + _offset);
	
	bool ret =  _file->write_endian(data, size, endian);
	
	_file->seek(old_offset);
	return ret;
}

bool Tea::FileSection::write_file(Tea::File& file, size_t size) {
	if(_file == nullptr) { return false; }
	
	size_t old_offset = _file->tell();
	_file->seek(_sect_offset + _offset);
	
	bool ret = _file->write_file(file, size);
	
	_file->seek(old_offset);
	return ret;
}
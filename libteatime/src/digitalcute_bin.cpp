#include <digitalcute.hpp>
#include "logging.hpp"
#include <string.h>

#include <filesystem>
namespace fs = std::filesystem;

#include "shiftjis.h"

std::string sj2utf8(const std::string& input)
{
	std::string output(3 * input.length(), ' '); //ShiftJis won't give 4byte UTF8, so max. 3 byte per input char are needed
	size_t indexInput = 0, indexOutput = 0;
	
	while(indexInput < input.length())
	{
		char arraySection = ((uint8_t)input[indexInput]) >> 4;
		
		size_t arrayOffset;
		if(arraySection == 0x8) arrayOffset = 0x100; //these are two-byte shiftjis
		else if(arraySection == 0x9) arrayOffset = 0x1100;
		else if(arraySection == 0xE) arrayOffset = 0x2100;
		else arrayOffset = 0; //this is one byte shiftjis
		
		//determining real array offset
		if(arrayOffset)
		{
			arrayOffset += (((uint8_t)input[indexInput]) & 0xf) << 8;
			indexInput++;
			if(indexInput >= input.length()) break;
		}
		arrayOffset += (uint8_t)input[indexInput++];
		arrayOffset <<= 1;
		
		//unicode number is...
		uint16_t unicodeValue = (shiftJIS_convTable[arrayOffset] << 8) | shiftJIS_convTable[arrayOffset + 1];
		
		//converting to UTF8
		if(unicodeValue < 0x80)
		{
			output[indexOutput++] = unicodeValue;
		}
		else if(unicodeValue < 0x800)
		{
			output[indexOutput++] = 0xC0 | (unicodeValue >> 6);
			output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
		}
		else
		{
			output[indexOutput++] = 0xE0 | (unicodeValue >> 12);
			output[indexOutput++] = 0x80 | ((unicodeValue & 0xfff) >> 6);
			output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
		}
	}
	
	output.resize(indexOutput); //remove the unnecessary bytes
	return output;
}

// reader class that will apply an xor pattern when reading data
class FileXor : public Tea::FileSection {
public:
	bool set_pattern(const uint8_t* pattern, size_t pattern_len) {
		if(!pattern)
			return false;
		
		_pattern.resize(pattern_len);
		memcpy(_pattern.data(), pattern, pattern_len);
		return true;
	}
	
	//TODO: implement writing
	
	bool read_endian(uint8_t* data, size_t size, Tea::Endian endian) override {
		size_t offset_start = this->tell();
		if(endian == Tea::Endian::current) { endian = _endian; }
		if(endian == Tea::Endian::big) {
			LOGERR("big endian not supported in digitalcute format cuz I'm lazy, and we don't need it");
			//if we do need it... be careful with endian swapping. xor should be applied before any endian swaps!
			return false;
		}
		
		bool ret = FileSection::read_endian(data, size, endian);
		
		if(_pattern.size() == 0)
			return ret;
		
		for(int i = 0; i < size; i++) {
			data[i] ^= _pattern[(i + offset_start) % _pattern.size()];
		}
		
		return ret;
	}
	
private:
	std::vector<uint8_t> _pattern;
};

bool DigitalcuteArchive::open_bin(Tea::File& infile) {
	uint32_t magic;
	infile.seek(0);
	
	Tea::File* file = &infile;
	file->read(magic);
	
	if(magic != 0x00045844) { //check for signature: DX\x4\x0
		if(magic == 0xC8EF1FE4) { //check for DX\x4\x0 signature, when XOR'd
			LOGVER("file has XOR pattern applied");
			FileXor* fxor = new FileXor();
			fxor->set_pattern((uint8_t*)u8"\xA0\x47\xEB\xC8\x94\xCA\x90\xB1\x1B\x1A\x23\x93", 12);
			fxor->open(infile, 0, infile.size());
			fxor->skip(4); //skip the magic, which we confirmed would be correct with xor applied
			file = fxor;
		}
		else {
			LOGERR("signature of file isn't the expected DX signature, neither with or without XOR pattern applied!");
			return false;
		}
	}
	
	uint32_t table_size;
	uint32_t unk0;
	uint32_t tableblock_offset;
	uint32_t table2_offset;
	uint32_t unk2;
	uint32_t unk3;
	file->read(table_size);
	file->read(unk0);
	file->read(tableblock_offset);
	file->read(table2_offset);
	file->read(unk2);
	file->read(unk3);
	
	
	file->seek(tableblock_offset + table2_offset + 32 + 8);
	while(true) {
		Entry ent;
		
		while(file->tell() % 4) { file->skip(1); }
		uint32_t eunk1;
		uint32_t name_offset; //offset from start of table 1
		uint32_t type;
		file->read(eunk1);
		file->read(name_offset);
		file->read(type);
		if(name_offset == 0) { break; }
		
		file->skip(16 + (20 - 4 - 4 - 4));
		uint32_t offset;
		uint32_t uncompressed_size;
		file->read(offset);
		file->read(uncompressed_size);
		
		ent.type = type;
		
		size_t cofs = file->tell();
		file->seek(tableblock_offset + name_offset);
		uint16_t unk;
		uint16_t ID;
		file->read(unk);
		file->read(ID);
		
		std::string sjstr1 = "";
		std::string sjstr2 = "";
		//TODO: make less ugly
		while(true) {
			char curr = file->read<uint8_t>();
			if(!curr) { break; }
			sjstr1 += curr;
		}
		while(file->tell() % 4) { file->skip(1); }
		while(true) {
			char curr = file->read<uint8_t>();
			if(!curr) { break; }
			sjstr2 += curr;
		}
		ent.name1 = sj2utf8(sjstr1);
		ent.name2 = sj2utf8(sjstr2);
		
		//read file into memory
		file->seek(offset + 28); //28 = header size
		Tea::FileMemory* tf = new Tea::FileMemory();
		tf->open_owned();
		tf->write_file(*file, uncompressed_size);
		ent.data = tf;
		
		file->seek(cofs);
		_filetable.push_back(ent);
	}
	
	
	/*//parse first table (we don't know entrycount yet, so we'll figure that out as we go)
	_filetable.resize(0);
	while(true) {
		Entry ent;
		while(file->tell() % 4) { file->skip(1); }
		uint16_t unk;
		uint16_t ID;
		file->read(unk);
		file->read(ID);
		ent.ID = ID;
		
		if(unk == 0 && ID == 0) {
			file->skip(-4);
			break;
		}
		
		LOGVER("now at %d", file->tell());
		
		std::string sjstr1 = "";
		std::string sjstr2 = "";
		//TODO: make less ugly
		while(true) {
			char curr = file->read<uint8_t>();
			if(!curr) { break; }
			sjstr1 += curr;
		}
		while(file->tell() % 4) { file->skip(1); }
		while(true) {
			char curr = file->read<uint8_t>();
			if(!curr) { break; }
			sjstr2 += curr;
		}
		
		ent.name1 = sj2utf8(sjstr1);
		ent.name2 = sj2utf8(sjstr2);
		
		_filetable.push_back(ent);
	}*/
	/*
	//now parse second table. this time, we do actually know the amount of entries
	file->seek(tableblock_offset + table2_offset + 32 + 8 + 4);
	for(int i = 0; i < _filetable.size(); i++) {
		uint32_t unk0;
		uint32_t unk1;
		uint32_t type;
		file->read(unk0);
		file->read(unk1);
		file->read(type);
		
		file->skip(20);
		
		uint32_t offset;
		uint32_t uncompressed_size; //just a guess!
		uint32_t compressed_size;
		file->read(offset);
		file->read(uncompressed_size);
		file->read(compressed_size);
		
		size_t offs = file->tell();
		file->seek(offset + 28);
		uint32_t real_size = (compressed_size == 0xFFFFFFFF) ? uncompressed_size : compressed_size;
		
		LOGVER("entry=%03d size=%-8d offset=%-8d name1=%s name2=%s", i, real_size, offset, _filetable[i].name1.c_str(), _filetable[i].name2.c_str());
		
		Tea::FileMemory* tf = new Tea::FileMemory();
		tf->open_owned();
		tf->write_file(*file, real_size);
		_filetable[i].data = tf;
		
		file->seek(offs);
	}*/
	
	
	if(file != &infile) //if file has XOR pattern (file ptr replaced with FileXor object), delete the object
		delete file;
	return true;
}

bool DigitalcuteArchive::write_dir(const std::string& outpath) {
	fs::create_directories(outpath);
	
	for(int i = 0; i < _filetable.size(); i++) {
		if(!_filetable[i].data)
			continue;
		
		fs::path out = outpath;
		out /= fs::u8path(_filetable[i].name1);
		Tea::FileDisk fd;
		fd.open(out.u8string().c_str(), Tea::Access_write);
		_filetable[i].data->seek(0);
		fd.write_file(*_filetable[i].data, _filetable[i].data->size());
		fd.close();
		
		LOGVER("wrote file %s, with size %d", _filetable[i].name1.c_str(), _filetable[i].data->size());
	}
	
	return true;
}





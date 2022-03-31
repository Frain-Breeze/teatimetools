#include <digitalcute.hpp>
#include "logging.hpp"
#include <string.h>

#include <functional>

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

bool decompressDigitalcute(Tea::File& infile, Tea::File& outfile) {
	
	size_t start_pos = infile.tell();
	
	if(start_pos == 158032812) { LOGERR("bla"); }
	
	uint32_t outsize;
	uint32_t insize;
	uint8_t delimit_char = 0;
	infile.read(outsize);
	infile.read(insize);
	infile.read(delimit_char);
	//LOGINF("delimiter: 0x%02x", delimit_char);
	
	if(outsize > 0x00FFFFFF) {
		LOGERR("massive file to decompress... are you sure? stopped for now");
		return false;
	}
	
	const size_t window_size = 0xFFFFFF;
	uint8_t* window = (uint8_t*)malloc(window_size);
	//uint8_t window[window_size];
	size_t window_progress = 0;
	
	size_t write_until = outfile.tell() + outsize;
	size_t read_until = start_pos + insize;
	while(outfile.tell() < write_until || infile.tell() < read_until) {
		uint8_t curr;
		infile.read(curr);
		
		if(curr == delimit_char) {
			uint8_t firstdata;
			infile.read(firstdata);
			int goback_size = firstdata & 0b00000011;
			bool is_length_16bit = firstdata & 0b00000100;
			
			
			if(goback_size == 0b00000011) {
				LOGERR("unknown decompression flag combination (full data=0x%02x) at position 0x%04X (%d)", firstdata, infile.tell() - start_pos, infile.tell() - start_pos);
				break;
			}
			
			int length = (firstdata >> 3) + 4;
			if(is_length_16bit) {
				length += int(infile.read<uint8_t>()) << (8-3);
			}
			
			int goback = infile.read<uint8_t>();
			for(int i = 0; i < goback_size; i++) {
				goback += int(infile.read<uint8_t>()) << ((i + 1) * 8);
			}
			goback++;
			
			//if(firstdata == 0x00 && goback == 1) { //goback == 1 because it's incremented by 1
			//	LOGWAR("got it! (at pos %d)", infile.tell() - start_pos);
			//}
			
			if(goback > window_size) {
				LOGERR("goback is %d, larger than the set window size (%d)", goback, window_size);
			}
			
			//printf("goback=%d,length=%d\n", goback, length);
			
			for(int i = 0; i < length; i++) {
				window[window_progress % window_size] = window[(window_progress - goback) % window_size];
				outfile.write(window[(window_progress - goback) % window_size]);
				//printf("%c", window[(window_progress - goback) % 0xFFFF]);
				window_progress++;
			}
		}
		else {
			window[window_progress % window_size] = curr;
			window_progress++;
			outfile.write(curr);
			//printf("%c", curr);
		}
	}
	
	//if(!(outfile.tell() < write_until)) { LOGVER("stopped because we reached the specified output size"); }
	//if(!(infile.tell() < read_until)) { LOGVER("stopped because we parsed the specified amount of input data"); }
	
	if((outfile.tell() < write_until) || (infile.tell() < read_until)) {
		LOGWAR("decompression couldn't be finished, because:");
		
		if(!(outfile.tell() < write_until)) { LOGWAR("we reached the specified output size, with input data left over"); }
		if(!(infile.tell() < read_until)) { LOGWAR("we ran out of input data, with leftover output space"); }
	}
	
	
	
	free(window);
	return true;
}

bool DigitalcuteArchive::open_bin(Tea::File& infile) {
	uint32_t magic;
	infile.seek(0);
	
	Tea::File* file = &infile;
	file->read(magic);
	
	FileXor fxor;
	if(magic != 0x00045844) { //check for signature: DX\x4\x0
		if(magic == 0xC8EF1FE4) { //check for DX\x4\x0 signature, when XOR'd
			LOGVER("file has XOR pattern applied");
			fxor.set_pattern((uint8_t*)"\xA0\x47\xEB\xC8\x94\xCA\x90\xB1\x1B\x1A\x23\x93", 12);
			fxor.open(infile, 0, infile.size());
			fxor.skip(4); //skip the magic, which we confirmed would be correct with xor applied
			file = &fxor;
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
	uint32_t table3_offset;
	uint32_t unk3;
	file->read(table_size);
	file->read(unk0);
	file->read(tableblock_offset); //string table
	file->read(table2_offset); //file entries + group names
	file->read(table3_offset); //"groups" or "directories"
	file->read(unk3);
	
	struct Group {
		uint32_t unk2 = 0;
		uint32_t linked_to = 0;
		uint32_t members = 0;
		uint32_t start_offset = 0;
		std::string name = "";
	};
	std::vector<Group> groups;
	
	file->seek(tableblock_offset + table2_offset + 44);
	int num_entries = 0;
	while(true) {
		uint32_t result = file->read<uint32_t>();
		if(result == 0) { break; }
		num_entries++;
		file->skip(40);
		//LOGVER("num entries: %d, offset = %d, result = %04x", num_entries, file->tell(), result);
	}
	
	if(!file->seek(tableblock_offset + table3_offset)) { LOGERR("couldn't seek to table 3"); return false; }
	int num_group_members = 0;
	while(true) {
		
		Group grp;
		
		file->read<uint32_t>(grp.unk2);
		file->read<uint32_t>(grp.linked_to);
		file->read<uint32_t>(grp.members);
		file->read<uint32_t>(grp.start_offset);
		
		num_group_members += grp.members;
		groups.push_back(grp);
		
		if(file->size() == file->tell() || num_group_members >= num_entries) { break; }
	}
	
	file->seek(tableblock_offset + table2_offset + 44);
	for(int i = 0; i < num_entries; i++) {
		Entry ent;
		
		while(file->tell() % 4) { file->skip(1); }
		size_t start_offset = file->tell();
		uint32_t name_offset; //offset from start of table 1
		Entry::Type type;
		file->read(name_offset);
		file->read(type);
		if(name_offset == 0) { break; }
		
		file->skip(16 + (20 - 4 - 4 - 4));
		uint32_t offset;
		uint32_t uncompressed_size;
		uint32_t compressed_size;
		file->read(offset);
		file->read(uncompressed_size);
		file->read(compressed_size);
		
		bool is_compressed = (compressed_size != 0xFFFFFFFF);
		size_t data_size = (is_compressed) ? compressed_size : uncompressed_size;
		
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
		
		uint32_t sofs = start_offset - (tableblock_offset + table2_offset);
		ent.group = -1;
		for(int g = 0; g < groups.size(); g++) {
			uint32_t next_offset = (g+1 < groups.size()) ? groups[g+1].start_offset : 0x7FFFFFFF;
			if((sofs < groups[g].start_offset) || (sofs > next_offset)) { continue; }
			ent.group = g;
		}
		
		LOGVER("group=%d size=%-8d offset=%-8d name1=%s name2=%s", ent.group, data_size, offset, ent.name1.c_str(), ent.name2.c_str());
		
		if(ent.type == Entry::Type::folder) {
			if(offset % 16) { LOGERR("entry %d's folder offset isn't aligned to 16 bytes (offset=%d)", i, offset); return false; }
			if((offset / 16) > groups.size()) {
				LOGERR("entry %d is assigned to group %d, but there are only %d groups", i, offset / 16, groups.size());
				return false;
			}
			groups[offset / 16].name = ent.name1;
		}
		else {
			//read file into memory
			file->seek(offset + 28); //28 = header size
			Tea::FileMemory* tf = new Tea::FileMemory();
			tf->open_owned();
			if(is_compressed) {
				Tea::FileSection sect;
				sect.open(*file, file->tell(), data_size);
				decompressDigitalcute(sect, *tf);
			}
			else {
				tf->write_file(*file, data_size);
			}
			ent.data = tf;
			
			_filetable.push_back(ent); //entry is only saved if it's not a folder
		}
		
		file->seek(cofs);
	}
	
	std::function<std::string(int)> assemble_path = [&](int group_index) -> std::string {
		if(group_index == -1) { return ""; }
		if(groups[group_index].linked_to != 0xFFFFFFFF && groups[group_index].linked_to != 0) {
			return (assemble_path(groups[group_index].linked_to / 16) + "/" + groups[group_index].name);
		}
		return groups[group_index].name;
	};
	
	for(int i = 0; i < _filetable.size(); i++) {
		fs::path newpath = assemble_path(_filetable[i].group);
		newpath /= _filetable[i].name1;
		_filetable[i].name1 = newpath.u8string();
		newpath = assemble_path(_filetable[i].group);
		newpath /= _filetable[i].name2;
		_filetable[i].name2 = newpath.u8string();
		//LOGVER("new path=%s", newpath.u8string().c_str());
	}
	
	return true;
}

bool DigitalcuteArchive::write_dir(const std::string& outpath) {
	fs::create_directories(outpath);
	
	for(int i = 0; i < _filetable.size(); i++) {
		if(!_filetable[i].data)
			continue;
		
		fs::path out = outpath;
		out /= fs::u8path(_filetable[i].name1);
		fs::create_directories(out.parent_path());
		Tea::FileDisk fd;
		fd.open(out.u8string().c_str(), Tea::Access_write);
		_filetable[i].data->seek(0);
		fd.write_file(*_filetable[i].data, _filetable[i].data->size());
		fd.close();
		
		LOGVER("wrote file %s, with size %d", _filetable[i].name1.c_str(), _filetable[i].data->size());
	}
	
	return true;
}





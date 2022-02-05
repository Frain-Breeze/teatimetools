#include "vridge_obj.hpp"

#include <vector>
#include <zlib.h>

bool VridgeObj::Entry::open(Tea::File& file, bool has_palette) {
	size_t start_offs = file.tell();
	
	uint32_t block_count = -1;
	file.read(block_count);
	if(block_count == -1) { LOGERR("block count could not be read!"); return false; }
	std::vector<uint32_t> block_offsets(block_count+1);
	file.read((uint8_t*)block_offsets.data(), block_offsets.size()*4);
	
	_subentries.resize(0);
	
	file.seek(start_offs + block_offsets[0]);
	for(int i = 0; i < block_count; i++) {
		
		if(block_offsets[i+1] == 0) { break; }
		if(block_offsets[i+1] < block_offsets[i]) {
			LOGWAR("next offset (%d) is smaller than the current one (%d), unknown what this means (stopping the read now)", block_offsets[i+1], block_offsets[i]);
			break;
		}
		
		LOGVER("size is 0x%08x (start at 0x%08x)", block_offsets[i+1] - block_offsets[i], file.tell());
		
		Tea::FileMemory* nf = new Tea::FileMemory();
		nf->open_owned();
		//HACK: is this really how it should be done?
		if(has_palette) { file.skip(4); nf->write_file(file, (block_offsets[i+1] - block_offsets[i]) - 4); }
		else { nf->write_file(file, block_offsets[i+1] - block_offsets[i]); }
		SubEntry sent;
		sent.data = nf;
		_subentries.push_back(sent);
		
	}
	
	while((file.tell() + 16) % 1024) { file.skip(1); }
	//TODO: check checksum
	uint64_t checksum_part1;
	uint64_t checksum_part2;
	file.endian(Tea::Endian::big);
	file.read(checksum_part1);
	file.read(checksum_part2);
	file.endian(Tea::Endian::little);
	LOGINF("checksum: 0x%08lx'%08lx, end located at: %08lx, %d subentries", checksum_part1, checksum_part2, file.tell(), _subentries.size());
	
	return true; //TODO: error checking
}


bool VridgeObj::open(Tea::File& file) {
	//TODO: make open function relative to current position
	
	file.seek(0);
	uint32_t first_u32;
	uint32_t second_u32;
	file.read(first_u32);
	file.read(second_u32);
	file.seek(0);
	
	int palette_size = 0;
	
	if(first_u32 == 0 || first_u32 > 0xFFFF|| second_u32 == 0 || second_u32 > 0xFFFF) {
		
		//HACK: there has to be some proper indicator for this... right?
		while(first_u32 == 0 || first_u32 > 0xFF|| second_u32 == 0 || second_u32 > 0xFF) {
			file.skip(64);
			file.read(first_u32);
			file.read(second_u32);
			file.skip(-8);
		}
		
		//if there's enough 16-color palettes to go up to 0x400... well, we'll figure something out when that happens
		if(!(file.tell() % 0x400)) { palette_size = 256; }
		else if(!(file.tell() % 0x40)) { palette_size = 16; }
	}
	else { palette_size = 0; }
	
	_entries.resize(0);
	do {
		Entry ent;
		ent.open(file, palette_size != 0);
		_entries.push_back(ent);
	} while(file.tell() < file.size());
	
	if(palette_size != 0) {
		file.seek(0);
		_palettes.resize(_entries.size());
		for(int i = 0; i < _palettes.size(); i++) {
			_palettes[i].resize(palette_size);
			file.read((uint8_t*)_palettes[i].data(), palette_size * 4);
		}
	}
	
	return true;
}

bool VridgeObj::export_entry(Tea::File& file, int entry) {
	for(int i = 0; i < _entries[entry]._subentries.size(); i++) {
		auto& cs = _entries[entry]._subentries[i];
		cs.data->seek(0);
		while(!cs.data->read<uint32_t>()) {}; //go to start of gzip stream
		if(cs.data->tell()) { cs.data->skip(-4); }
		cs.data->size();
		
		uint8_t block_in[256];
		uint8_t block_out[256];
		
		z_stream zs;
		zs.zalloc = Z_NULL;
		zs.zfree = Z_NULL;
		zs.opaque = Z_NULL;
		zs.avail_in = 0;
		zs.next_in = Z_NULL;
		int ret = inflateInit2(&zs, 16 | MAX_WBITS);
		
		do {
			zs.avail_in = ((cs.data->size() - cs.data->tell()) > 256) ? 256 : (cs.data->size() - cs.data->tell()); //read chunk size (or until end of file if there's less than a whole buffer left)
			cs.data->read(block_in, zs.avail_in);
			if(zs.avail_in == 0)
				break;
			zs.next_in = block_in;
			
			do {
				zs.avail_out = 256;
				zs.next_out = block_out;
				ret = inflate(&zs, Z_NO_FLUSH);
				
				int have = 256 - zs.avail_out;
				file.write(block_out, have);
			} while(zs.avail_out == 0);
		} while(ret != Z_STREAM_END);
		
		inflateEnd(&zs);
	}
	
	return true;
}
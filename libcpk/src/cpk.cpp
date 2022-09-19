#include "cpk.hpp"

#include <string.h>
#include <string>
#include <typeinfo>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <array>
namespace fs = std::filesystem;

#include <logging.hpp>

// https://github.com/ConnorKrammer/cpk-tools/blob/master/LibCPK/CPK.cs

uint16_t decompress_get_next_bits(uint8_t* in_data, int& offset, uint8_t& bit_pool, int& bits_left, int bit_count) {
    uint16_t out_bits = 0;
    int bits_produced = 0;
    int bits_this_round;
    
    while(bits_produced < bit_count) {
        if(bits_left == 0) {
            bit_pool = in_data[offset];
            bits_left = 8;
            offset--;
        }
        
        if(bits_left > (bit_count - bits_produced))
            bits_this_round = bit_count - bits_produced;
        else
            bits_this_round = bits_left;
        
        out_bits <<= bits_this_round;
        
        out_bits |= (uint16_t)((uint16_t)(bit_pool >> (bits_left - bits_this_round)) & ((1 << bits_this_round) - 1));
        
        bits_left -= bits_this_round;
        bits_produced += bits_this_round;
    }
    
    return out_bits;
}

bool decompress_crilayla(Tea::File& in_file, size_t in_size, Tea::File& out_file) {
    
    size_t in_offset = in_file.tell();
    
    in_file.skip(8); //TODO: check for CRILAYLA
    uint32_t uncomp_size, uncomp_header_offset;
    in_file.read(uncomp_size);
    in_file.read(uncomp_header_offset);
    
    std::vector<uint8_t> buffer(in_size);
    std::vector<uint8_t> out_buffer(uncomp_size + 0x100);
    
    in_file.seek(in_offset);
    in_file.read(buffer.data(), in_size);
    
    in_file.seek(in_offset + uncomp_header_offset + 16);
    in_file.read(out_buffer.data(), 0x100);
    
    int input_end = in_size - 0x100 - 1;
    int input_offset = input_end;
    int output_end = 0x100 + uncomp_size - 1;
    uint8_t bit_pool = 0;
    int bits_left = 0, bytes_output = 0;
    std::array<int, 4> vle_lens = { 2, 3, 5, 8 };
    
    while(bytes_output < uncomp_size) {
        if (decompress_get_next_bits(buffer.data(), input_offset, bit_pool, bits_left, 1) > 0) {
            int backreference_offset = output_end - bytes_output + decompress_get_next_bits(buffer.data(), input_offset, bit_pool, bits_left, 13) + 3;
            int backreference_length = 3;
            int vle_level;

            for (vle_level = 0; vle_level < vle_lens.size(); vle_level++)
            {
                uint16_t this_level = decompress_get_next_bits(buffer.data(), input_offset, bit_pool, bits_left, vle_lens[vle_level]);
                backreference_length += this_level;
                if (this_level != ((1 << vle_lens[vle_level]) - 1)) { break; }
            }

            if (vle_level == vle_lens.size())
            {
                uint16_t this_level;
                do
                {
                    this_level = decompress_get_next_bits(buffer.data(), input_offset, bit_pool, bits_left, 8);
                    backreference_length += this_level;
                } while (this_level == 255);
            }

            for (int i = 0; i < backreference_length; i++)
            {
                out_buffer[output_end - bytes_output] = out_buffer[backreference_offset--];
                //printf("%02x", out_buffer[output_end - bytes_output]);
                bytes_output++;
            }
        }
        else {
            // verbatim byte
            out_buffer[output_end - bytes_output] = decompress_get_next_bits(buffer.data(), input_offset, bit_pool, bits_left, 8);
            //printf("%02x", out_buffer[output_end - bytes_output]);
            //LOGINF("decompressed byte %02x to position %08x", out_buffer[output_end - bytes_output], output_end - bytes_output);
            bytes_output++;
        }
    }
    out_file.write(out_buffer.data(), out_buffer.size());
    
    return true;
}

void decryptUTF(std::vector<uint8_t>& data) {
    uint32_t m = 0x0000655f;
    uint32_t t = 0x00004115;

    uint8_t d;

    for(size_t i = 0; i < data.size(); i++) {
        d = data[i];
        d = d ^ (m & 0xFF);
        data[i] = d;
        m *= t;
    }
    
    
}

void readUTF(Tea::File& file, std::vector<uint8_t>& out_data) {
    uint32_t unk1; file.read(unk1);
    uint64_t utf_size = -1;
	if(!file.read(utf_size)) { LOGERR("read bad"); return; }
    out_data.resize(utf_size);
    file.read(out_data.data(), utf_size);
    if(out_data[0] != 0x40 || out_data[1] != 0x55 || out_data[2] != 0x54 || out_data[3] != 0x46) { //@UTF
        decryptUTF(out_data);
    }
}

bool CPK::save(Tea::File& file) {
	file.seek(0);
	file.write((uint8_t*)"CPK ", 4);
	file.write((uint8_t*)"\xff\0\0\0", 4);
	
	file.seek(0x800 - 6);
	file.write((uint8_t*)"(c)CRI", 6);
	
	UTF_Table tcpk; tcpk._table_name = "CpkHeader";
	UTF_Table ttoc; bool use_toc = false; ttoc._table_name = "CpkTocInfo";
	UTF_Table tetoc; bool use_etoc = false; tetoc._table_name = "CpkEtocInfo";
	UTF_Table titoc; bool use_itoc = false; titoc._table_name = "CpkItocInfo";
	UTF_Table titoc_datah; titoc_datah._table_name = "CpkItocH";
	UTF_Table titoc_datal; titoc_datal._table_name = "CpkItocL";
	
	//TODO: implement? find game that uses htoc and/or gtoc
	bool use_htoc = false;
	bool use_gtoc = false;
	bool use_hgtoc = false;
	
	//default initialization values
	{
#define TF(type, storage, string) UTF_Table::Column(UTF_Table::Flags(UTF_Table::Flags::Type::type, UTF_Table::Flags::Storage::storage), string)
#define CE(type, storage, string) tcpk._columns.push_back(TF(type, storage, string))
	tcpk._columns.resize(0);
	CE(i64, per_row, "UpdateDateTime");
	CE(i64, zero, "FileSize");
	CE(i64, per_row, "ContentOffset");
	CE(i64, per_row, "ContentSize");
	CE(i64, per_row, "TocOffset");
	CE(i64, per_row, "TocSize");
	CE(i32, zero, "TocCrc");
	CE(i64, per_row, "HtocOffset");
	CE(i64, per_row, "HtocSize"); //TODO: do htoc and etoc and hgtoc also have CRC? very likely, but should find a game that confirms this
	//CE(i32, zero, "HtocCrc");
	CE(i64, per_row, "EtocOffset");
	CE(i64, per_row, "EtocSize");
	//CE(i32, zero, "EtocCrc");
	CE(i64, per_row, "ItocOffset");
	CE(i64, per_row, "ItocSize");
	CE(i32, zero, "ItocCrc");
	CE(i64, per_row, "GtocOffset");
	CE(i64, per_row, "GtocSize");
	CE(i32, zero, "GtocCrc");
	CE(i64, per_row, "HgtocOffset");
	CE(i64, per_row, "HgtocSize");
	//CE(i32, zero, "HgtocCrc");
	CE(i64, per_row, "EnabledPackedSize");
	CE(i64, per_row, "EnabledDataSize");
	CE(i64, zero, "TotalDataSize");
	CE(i32, zero, "Tocs");
	CE(i32, per_row, "Files");
	CE(i32, per_row, "Groups");
	CE(i32, per_row, "Attrs");
	CE(i32, zero, "TotalFiles");
	CE(i32, zero, "Directories");
	CE(i32, zero, "Updates");
	CE(i16, per_row, "Version");
	CE(i16, per_row, "Revision");
	CE(i16, per_row, "Align");
	CE(i16, per_row, "Sorted");
	CE(i16, per_row, "EnableFileName"); //HACK: figure out when this should be zero (to avoid crashes)
	CE(i16, zero, "EID");
	CE(i32, per_row, "CpkMode");
	CE(str, per_row, "Tvers");
	CE(str, zero, "Comment");
	CE(i32, per_row, "Codec");
	CE(i32, per_row, "DpkItoc");
	tcpk.resize(tcpk.num_columns(), 1);
#undef CE
#define TE(type, storage, string) ttoc._columns.push_back(TF(type, storage, string))
	ttoc._columns.resize(0);
	TE(str, per_row, "DirName");
	TE(str, per_row, "FileName");
	TE(i32, per_row, "FileSize");
	TE(i32, per_row, "ExtractSize");
	TE(i64, per_row, "FileOffset");
	TE(i32, per_row, "ID");
	TE(str, constant, "UserString");
#undef TE
#define EE(type, storage, string) tetoc._columns.push_back(TF(type, storage, string))
	tetoc._columns.resize(0);
	EE(i64, per_row, "UpdateDateTime");
	EE(str, per_row, "LocalDir");
#undef EE
#define IE(type, storage, string) titoc._columns.push_back(TF(type, storage, string))
	titoc._columns.resize(0);
	IE(i32, per_row, "FilesL");
	IE(i32, per_row, "FilesH");
	IE(data, per_row, "DataL");
	IE(data, per_row, "DataH");
#undef IE
#define IDH(type, storage, string) titoc_datah._columns.push_back(TF(type, storage, string))
	IDH(i16, per_row, "ID");
	IDH(i32, per_row, "FileSize");
	IDH(i32, per_row, "ExtractSize");
#undef IDH
#define IDL(type, storage, string) titoc_datal._columns.push_back(TF(type, storage, string))
	IDL(i16, zero, "ID"); //IDL(i16, per_row, "ID");
	IDL(i16, zero, "FileSize"); //IDL(i32, per_row, "FileSize");
	IDL(i16, zero, "ExtractSize"); //IDL(i32, per_row, "ExtractSize");
#undef IDH
#undef TF
	}
	
	
	uint64_t current_offset = 2048; //be careful: this is *relative*
	uint64_t content_offset = 2048;
	tcpk.set_by_name<uint64_t>(content_offset, "ContentOffset", 2048); //HACK: should be dynamic?
	size_t current_data_size = 0;
	//first pass: do all ID-bound files (put in DataH)
	size_t curr_id_file = 0;
	for(int i = 0; i < _filetable.size(); i++) {
		if(!_filetable[i].is_id_based) { continue; }
		use_itoc = true;
		
		titoc_datah.resize(titoc_datah.num_columns(), curr_id_file+1);
		titoc_datah.set_by_name<uint16_t>(_filetable[i].ID, "ID", curr_id_file);
		titoc_datah.set_by_name<uint32_t>(_filetable[i].file->size(), "FileSize", curr_id_file);
		titoc_datah.set_by_name<uint32_t>(_filetable[i].file->size(), "ExtractSize", curr_id_file);
		
		file.seek(current_offset);
		_filetable[i].file->seek(0);
		file.write_file(*_filetable[i].file, _filetable[i].file->size());
		
		current_offset += _filetable[i].file->size();
		current_data_size += _filetable[i].file->size();
		
		curr_id_file++;
	}
	
	//TODO: second/third pass (do DataL and normal name-bound files)
	size_t curr_name_file = 0;
	for(int i = 0; i < _filetable.size(); i++) {
		if(_filetable[i].is_id_based) { continue; }
		use_toc = true;
		//use_etoc = true; //TODO: etoc
		ttoc.resize(ttoc.num_columns(), curr_name_file+1);
		//TODO: add filename, dir, etc
		ttoc.set_by_name(strdup(_filetable[i].filename.c_str()), "FileName", curr_name_file);
		ttoc.set_by_name(strdup(_filetable[i].dirname.c_str()), "DirName", curr_name_file);
		ttoc.set_by_name(strdup(_filetable[i].userstring.c_str()), "UserString", curr_name_file);
		ttoc.set_by_name(_filetable[i].ID, "ID", curr_name_file);
		ttoc.set_by_name<uint32_t>(current_offset - 2048, "FileOffset", curr_name_file);
		ttoc.set_by_name<uint32_t>(_filetable[i].file->size(), "FileSize", curr_name_file);
		ttoc.set_by_name<uint32_t>(_filetable[i].file->size(), "ExtractSize", curr_name_file);
		
		file.seek(current_offset);
		_filetable[i].file->seek(0);
		file.write_file(*_filetable[i].file, _filetable[i].file->size());
		
		current_offset += _filetable[i].file->size();
		current_data_size += _filetable[i].file->size();
		
		curr_name_file++;
	}
	
	size_t end_of_data = current_offset;
	LOGVER("total data size (excluding padding): %d", current_data_size);
	tcpk.set_by_name<uint64_t>(current_data_size, "EnabledPackedSize", 0);
	tcpk.set_by_name<uint64_t>(current_data_size, "EnabledDataSize", 0);
	
	tcpk._columns[tcpk.get_column("GtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
	tcpk._columns[tcpk.get_column("GtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
	if(!use_toc) {
		tcpk._columns[tcpk.get_column("TocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
		tcpk._columns[tcpk.get_column("TocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
		(*tcpk("EnableFileName", 0)).data.i16 = 0;
	}
	else { (*tcpk("EnableFileName", 0)).data.i16 = 1; }
	
	if(!use_etoc) {
		tcpk._columns[tcpk.get_column("EtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
		tcpk._columns[tcpk.get_column("EtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
	}
	if(!use_itoc) {
		tcpk._columns[tcpk.get_column("ItocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
		tcpk._columns[tcpk.get_column("ItocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
	}
	if(!use_gtoc) {
		tcpk._columns[tcpk.get_column("GtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
		tcpk._columns[tcpk.get_column("GtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
	}
	if(!use_htoc) {
		tcpk._columns[tcpk.get_column("HtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
		tcpk._columns[tcpk.get_column("HtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
	}
	if(!use_hgtoc) {
		tcpk._columns[tcpk.get_column("HgtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
		tcpk._columns[tcpk.get_column("HgtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
	}
	
	tcpk.set_by_name<uint32_t>(_filetable.size(), "Files", 0);
	tcpk.set_by_name<uint32_t>(0, "Groups", 0);
	tcpk.set_by_name<uint32_t>(0, "Attrs", 0);
	
	tcpk.set_by_name<uint64_t>(current_data_size, "EnabledPackedSize", 0);
	tcpk.set_by_name<uint64_t>(current_data_size, "EnabledDataSize", 0);
	
	tcpk.set_by_name<uint16_t>(7, "Version", 0);
	tcpk.set_by_name<uint16_t>((use_toc) ? 2 : 14, "Revision", 0); //set version to 7rev2 for file-based archives and 7rev14 for ID-based ones. I'm not sure if this is important, but it's what my reference archives use
	
	tcpk.set_by_name<uint16_t>(2048, "Align", 0);
	tcpk.set_by_name<uint16_t>(0, "Sorted", 0); //TODO: what type of sort is this?
	tcpk.set_by_name<uint32_t>((use_toc) ? 1 : 0, "CpkMode", 0);
	
	char* vers_str = (char*)malloc(strlen("teatimetools LibCPK version something") + 1);
	strcpy(vers_str, "teatimetools LibCPK version something");
	tcpk.set_by_name<char*>(vers_str, "Tvers", 0);
	
	tcpk.set_by_name<uint32_t>(0, "Codec", 0);
	tcpk.set_by_name<uint32_t>(0, "DpkItoc", 0);
	
	//TODO: save ETOC, GTOC, HTOC, HGTOC, and all other icecream flavors
	
	tcpk.set_by_name<uint64_t>(file.tell() - content_offset, "ContentSize", 0);
	tcpk.set_by_name<uint64_t>(content_offset, "ContentOffset", 0);
	
	if(use_toc) {
		while(file.tell() % 2048) { file.skip(1); }
		LOGVER("saving TOC at position %d", file.tell());
		tcpk.set_by_name<uint64_t>(file.tell(), "TocOffset", 0);
		size_t before_toc_packet = file.tell();
		
		file.write((uint8_t*)"TOC \xff\0\0\0", 8);
		file.skip(8); //TOC packet size
		
		size_t before_toc = file.tell();
		ttoc.save(file);
		while(file.tell() % 16) { file.write<uint8_t>('\0'); }
		
		uint64_t toc_size = file.tell() - before_toc;
		
		size_t after_toc = file.tell();
		
		tcpk.set_by_name<uint64_t>(file.tell() - before_toc_packet, "TocSize", 0);
		
		file.seek(-toc_size - 8, Tea::Seek_current);
		file.write<uint64_t>(toc_size);
		
		file.seek(after_toc);
	}
	if(use_itoc) {
		titoc.resize(titoc.num_columns(), 1);
		{
			LOGVER("saving ITOC DataL");
			UTF_Table::Unit::Data::Data_type data;
			
			Tea::FileMemory memf; memf.open_owned();
			titoc_datal.save(memf); memf.seek(0);
			data.len = memf.size();
			data.data = (uint8_t*)malloc(memf.size());
			memf.read(data.data, data.len);
			titoc.set_by_name(data, "DataL", 0);
			titoc.set_by_name<uint32_t>(titoc_datal.num_rows(), "FilesL", 0);
		}
		{
			LOGVER("saving ITOC DataH");
			UTF_Table::Unit::Data::Data_type data;
			
			Tea::FileMemory memf; memf.open_owned();
			
			titoc_datah.save(memf); memf.seek(0);
			data.len = memf.size();
			data.data = (uint8_t*)malloc(memf.size());
			memf.read(data.data, data.len);
			titoc.set_by_name(data, "DataH", 0);
			titoc.set_by_name<uint32_t>(titoc_datah.num_rows(), "FilesH", 0);
		}
		while(file.tell() % 2048) { file.skip(1); }
		{
			LOGVER("saving ITOC at position %d", file.tell());
			tcpk.set_by_name<uint64_t>(file.tell(), "ItocOffset", 0);
			size_t before_itoc_packet = file.tell();
			
			file.write((uint8_t*)"ITOC\xff\0\0\0", 8);
			file.skip(8); //TOC packet size
			
			size_t before_itoc = file.tell();
			titoc.save(file); //TODO: continue, same with DataL/DataH etc
			while(file.tell() % 8) { file.write<uint8_t>('\0'); }
			uint64_t itoc_size = file.tell() - before_itoc;
			
			tcpk.set_by_name<uint64_t>(file.tell() - before_itoc_packet, "ItocSize", 0);
			
			file.seek(-itoc_size - 8, Tea::Seek_current);
			file.write<uint64_t>(itoc_size);
		}
	}
	
	//paste CPK header into file
	file.seek(16);
	LOGVER("saving header at position %d", file.tell());
	tcpk.save(file);
	uint64_t cpk_table_size = file.tell() - 16;
	file.seek(8); //move to "table size" portion
	file.write<uint64_t>(cpk_table_size);
	return true;
}

bool CPK::save() {
    if(!_file)
        return false;
    return this->save(*_file);
}

bool CPK::close() {
    //TODO: finish
    return false;
}

bool CPK::save_directory(const std::string& directory) {
	//TODO: error stuff on directory etc
	fs::create_directories(directory);
	
	for(int i = 0; i < _filetable.size(); i++) {
		fs::path out_path = directory;
		out_path /= _filetable[i].dirname;
		fs::create_directories(out_path);
		if(_filetable[i].is_id_based) {
			char idname[256];
			snprintf(idname, 256, "__ID_%04d.bin", _filetable[i].ID);
			out_path /= (std::string)idname;
		}
		else { out_path /= _filetable[i].filename; }
		
		Tea::FileDisk f;
		f.open(out_path.u8string().c_str(), Tea::Access_write);
		
		_filetable[i].file->seek(0);
		f.write_file(*_filetable[i].file, _filetable[i].file->size());
		
		f.close();
		
		LOGVER("saved file %s (ID %d) with size %ld", _filetable[i].filename.c_str(), _filetable[i].ID, _filetable[i].file->size());
	}
	
	return true;
}

bool CPK::open_directory(const std::string& directory) {
	this->open_empty();
	
	fs::path fs_dir = fs::u8path(directory);
	std::vector<fs::path> files;
	
	std::function<void(fs::path, std::vector<fs::path>&)> sort_dir = [&](fs::path cur_path, std::vector<fs::path>& output) -> void{
		LOGVER("starting sort of %s path (filename %s)", cur_path.u8string().c_str(), cur_path.filename().u8string().c_str());
		LOGBLK
		std::vector<fs::path> local_files;
		for(const auto& p : fs::directory_iterator(cur_path)) { local_files.push_back(p.path()); }
		
		
		std::sort(local_files.begin(), local_files.end(), [](fs::path& a, fs::path& b){
			//HACK: filenames shouldn't be empty
			if(b.filename().empty()) {
				return false;
			}
			if(a.filename().empty()) {
				return true;
			}
			std::string first = a.filename().u8string();
			std::string second = b.filename().u8string();
			for(int i = 0; i < first.length() && i < second.length(); i++) {
				if(first[i] <= 'Z' && first[i] >= 'A') { first[i] += 32; }
				if(second[i] <= 'Z' && second[i] >= 'A') { second[i] += 32; }
				if(first[i] < second[i]) {
					//LOGINF("%s < %s", first.c_str(), second.c_str());
					return true;
				}
				else if(first[i] > second[i]) {
					//LOGINF("%s > %s", first.c_str(), second.c_str());
					return false;
				}
			}
			//LOGINF("%s = %s", first.c_str(), second.c_str());
			return true;
		});
		
		for(int i = 0; i < local_files.size(); i++) {
			if(fs::is_directory(local_files[i])) {
				sort_dir(local_files[i], output);
			}
			else if(fs::is_regular_file(local_files[i])) {
				output.push_back(local_files[i]);
				LOGVER("entry %5d is %s", i, local_files[i].u8string().c_str());
			}
			else {
				LOGERR("path %s is not a normal file or directory!", local_files[i].u8string().c_str());
			}
		}
	};
	
	sort_dir(fs_dir, files); //highly doubt this is really required... but might as well, right?
	
	for(int i = 0; i < files.size(); i++) {
		fs::path& p = files[i];
		fs::path rel_dir_path = fs::relative(p, fs_dir).parent_path();
		
		int scanned_id = -1;
		if(sscanf(p.filename().u8string().c_str(), "__ID_%04d.bin", &scanned_id) != 1) { scanned_id = -1; }
		
		Entry ent;
		if(scanned_id != -1) {
			ent.is_id_based = true;
			ent.ID = scanned_id;
		}
		else {
			ent.ID = i; //HACK: this should probably care about whether the ID has been used by an ID-based file
			ent.is_id_based = false;
			ent.filename = p.filename().u8string();
			ent.dirname = rel_dir_path.u8string();
		}
		
		Tea::FileDisk nfile;
		nfile.open(p.u8string().c_str(), Tea::Access_read);
		
		Tea::FileMemory* memf = new Tea::FileMemory(); //we need to use a memory file, cuz we run out of file descriptors otherwise
		memf->open_owned();
		memf->write_file(nfile, nfile.size());
		
		nfile.close();
		
		ent.file = memf;
		_filetable.push_back(ent);
	}
	
	return true;
}

bool CPK::open_empty(Tea::File& file) {
    this->close();
    _file = &file;
    return this->open_empty();
}

bool CPK::open_empty() {
	return true;
} //HACK: to make it compile

bool CPK::open(Tea::File& file) {
	this->close();
	
	int processed_files = 0;
	int processed_bytes = 0;
	
	_file = &file;
	
	char magic[5] = "\0\0\0\0";
	if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading CPK magic"); return false; }
	if(strncmp(magic, "CPK ", 4)) { LOGERR("expected CPK magic, but didn't get it"); return false; }
	
	std::vector<uint8_t> CPK_packet;
	readUTF(*_file, CPK_packet);
	Tea::FileMemory cpk_file_packet(*CPK_packet.data(), CPK_packet.size());
	UTF_Table tcpk; tcpk.open(cpk_file_packet);
	
	_filetable.resize(0); //reset all files
	
	{ //read TOC and open files
		int toc_offset_column = tcpk.get_column("TocOffset");
		int toc_size_column = tcpk.get_column("TocSize");
		
		if(toc_offset_column == -1 || toc_size_column == -1) { LOGERR("TocOffset or TocSize not found in CPK header"); return false; }
		uint64_t toc_offset = -1;
		if(!tcpk.get(toc_offset, toc_offset_column, 0)) {
			LOGINF("TocOffset could not be read from table, could be an ID-based file");
			goto after_toc;
		}
		
		_file->seek(toc_offset);
		LOGVER("toc offset: %d", toc_offset);
		
		if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading TOC magic"); return false; }
		if(strncmp(magic, "TOC ", 4)) { LOGERR("expected TOC magic, but didn't get it"); return false; }
		std::vector<uint8_t> toc_packet;
		readUTF(*_file, toc_packet);
		Tea::FileMemory toc_file_packet(*toc_packet.data(), toc_packet.size());
		UTF_Table ttoc; ttoc.open(toc_file_packet);
		
		for(int i = 0; i < ttoc.num_rows(); i++) {
			Entry ent;
			char* tmp_ptr = nullptr;
			ttoc.get_by_name(tmp_ptr, "DirName", i); if(tmp_ptr) { ent.dirname = tmp_ptr; }
			ttoc.get_by_name(tmp_ptr, "FileName", i); if(tmp_ptr) { ent.filename = tmp_ptr; }
			uint32_t filesize;
			uint32_t extractsize;
			ttoc.get_by_name(filesize, "FileSize", i);
			ttoc.get_by_name(extractsize, "ExtractSize", i);
			uint32_t file_offset = -1;
			ttoc.get_by_name(file_offset, "FileOffset", i);
			ttoc.get_by_name(tmp_ptr, "UserString", i); if(tmp_ptr) { ent.userstring = tmp_ptr; }
			ttoc.get_by_name(ent.ID, "ID", i);
			
			if(file_offset == -1)
				LOGERR("offset of file %s (%d) is invalid", ent.filename.c_str(), i);
			
			LOGVER("size %010d, extractsize %010d, dir %s, name %s", filesize, extractsize, ent.dirname.c_str(), ent.filename.c_str());
			
			Tea::FileMemory* mem = new Tea::FileMemory();
			mem->open_owned();
			_file->seek(file_offset + 2048);
			char cri_check[9] = "notcrill";
			if(filesize >= 16) {
				_file->read((uint8_t*)cri_check, 8);
				_file->seek(-8, Tea::Seek_current);
			}
			//TODO: also use extractsize vs filesize to check for compression
			if(!memcmp(cri_check, "CRILAYLA", 8)) { //check if file is CRILAYLA compressed
				decompress_crilayla(*_file, filesize, *mem);
			}
			else {
				mem->write_file(*_file, filesize);
			}
			
			ent.file = mem;
			
			processed_files++;
			processed_bytes += extractsize;
			
			_filetable.push_back(ent);
		}
	}
after_toc:
	
	{ //read ETOC
		int etoc_offset_column = tcpk.get_column("EtocOffset");
		int etoc_size_column = tcpk.get_column("EtocSize");
		
		if(etoc_offset_column == -1 || etoc_size_column == -1) { LOGERR("EtocOffset or EtocSize not found in CPK header"); return false; }
		uint64_t etoc_offset = -1;
		if(!tcpk.get(etoc_offset, etoc_offset_column, 0)) {
			LOGINF("EtocOffset could not be read from table, could be an ID-based file");
			goto after_etoc;
		}
		
		_file->seek(etoc_offset);
		if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading ETOC magic"); return false; }
		if(strncmp(magic, "ETOC", 4)) { LOGERR("expected ETOC magic, but didn't get it"); return false; }
		std::vector<uint8_t> etoc_packet;
		readUTF(*_file, etoc_packet);
		Tea::FileMemory etoc_file_packet(*etoc_packet.data(), etoc_packet.size());
		UTF_Table tetoc; tetoc.open(etoc_file_packet);
		
		//TODO: continue I guess
	}
after_etoc:
	
	{ //read ITOC
		int itoc_offset_column = tcpk.get_column("ItocOffset");
		int itoc_size_column = tcpk.get_column("ItocSize");
		
		if(itoc_offset_column == -1 || itoc_size_column == -1) { LOGERR("ItocOffset or ItocSize not found in CPK header"); return false; }
		uint64_t itoc_offset = -1;
		if(!tcpk.get(itoc_offset, itoc_offset_column, 0)) {
			LOGINF("ItocOffset could not be read from table, could be a filename-based file");
			goto after_itoc;
		}
		
		_file->seek(itoc_offset);
		if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading ITOC magic"); return false; }
		if(strncmp(magic, "ITOC", 4)) { LOGERR("expected ITOC magic, but didn't get it"); return false; }
		std::vector<uint8_t> itoc_packet;
		readUTF(*_file, itoc_packet);
		Tea::FileMemory itoc_file_packet(*itoc_packet.data(), itoc_packet.size());
		UTF_Table titoc; titoc.open(itoc_file_packet);
		
		//not really that clean, to say the least
		UTF_Table::Unit::Data::Data_type data_l;
		UTF_Table::Unit::Data::Data_type data_h;
		titoc.get_by_name(data_l, "DataL", 0);
		titoc.get_by_name(data_h, "DataH", 0);
		Tea::FileMemory f_data_l;
		Tea::FileMemory f_data_h;
		f_data_l.open(data_l.data, data_l.len);
		f_data_h.open(data_h.data, data_h.len);
		UTF_Table u_data_l;
		UTF_Table u_data_h;
		u_data_l.open(f_data_l);
		u_data_h.open(f_data_h);
		
		uint32_t content_offset = -1;
		tcpk.get_by_name(content_offset, "ContentOffset", 0);
		
		uint32_t content_processed = 0;
		
		struct Tmp_entry {
			uint32_t ID;
			uint32_t size;
			uint32_t extract_size;
			bool in_data_h; //otherwise in data_l
		};
		std::vector<Tmp_entry> tmp_entries;
		
		for(size_t i = 0; i < u_data_l.num_rows(); i++) {
			Tmp_entry ent;
			bool ok = true;
			ok &= u_data_l.get_by_name(ent.ID, "ID", i);
			ok &= u_data_l.get_by_name(ent.size, "FileSize", i);
			ok &= u_data_l.get_by_name(ent.extract_size, "ExtractSize", i);
			ent.in_data_h = false;
			tmp_entries.push_back(ent);
		}
		
		for(size_t i = 0; i < u_data_h.num_rows(); i++) {
			Tmp_entry ent;
			bool ok = true;
			ok &= u_data_h.get_by_name(ent.ID, "ID", i);
			ok &= u_data_h.get_by_name(ent.size, "FileSize", i);
			ok &= u_data_h.get_by_name(ent.extract_size, "ExtractSize", i);
			ent.in_data_h = true;
			tmp_entries.push_back(ent);
		}
		
		std::sort(tmp_entries.begin(), tmp_entries.end(), [&](const Tmp_entry& a, const Tmp_entry& b) -> bool { return (a.ID < b.ID); });
		
		for(size_t i = 0; i < tmp_entries.size(); i++) {
			Entry ent;
			ent.ID = tmp_entries[i].ID;
			ent.is_id_based = true;
			
			Tea::FileMemory* mem = new Tea::FileMemory();
			mem->open_owned();
			
			_file->seek(content_offset + content_processed);
			char cri_check[9] = "notcrill";
			if(tmp_entries[i].size >= 16) {
				_file->read((uint8_t*)cri_check, 8);
				_file->seek(-8, Tea::Seek_current);
			}
			
			if(tmp_entries[i].size != tmp_entries[i].extract_size && !memcmp(cri_check, "CRILAYLA", 8)) {
				decompress_crilayla(*_file, tmp_entries[i].size, *mem);
			}
			else {
				mem->write_file(*_file, tmp_entries[i].size);
			}
			ent.file = mem;
			
			processed_files++;
			processed_bytes += tmp_entries[i].extract_size;
			
			content_processed += tmp_entries[i].size;
			while(content_processed % 2048) {
				content_processed++;
			}
			LOGINF("new ID %d with size %d, compressed %d (offset is now %d / 0x%08x)", tmp_entries[i].ID,  tmp_entries[i].extract_size, tmp_entries[i].size, content_offset + content_processed, content_offset + content_processed);
			_filetable.push_back(ent);
		}
	}
after_itoc:
	
	{
		LOGWAR("ignoring possible GTOC section!"); //TODO: implement GTOC
	}
after_gtoc:
	
	LOGOK("processed %d files with a combined size of %d bytes", processed_files, processed_bytes);
	return true;
}

CPK::~CPK() {
	for(auto& ent : _filetable) {
		if(ent.file) {
			delete ent.file;
			ent.file = nullptr;
		}
	}
}
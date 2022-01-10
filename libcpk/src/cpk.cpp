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
	
	UTF_Table tcpk; tcpk._table_name = "CpkHeader";;
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
		ttoc.set_by_name(_filetable[i].filename.c_str(), "FileName", curr_name_file);
		ttoc.set_by_name(_filetable[i].dirname.c_str(), "DirName", curr_name_file);
		ttoc.set_by_name(_filetable[i].userstring.c_str(), "UserString", curr_name_file);
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

/*bool CPK::save(Tea::File& file) {
    //write CPK header
    file.seek(0);
    file.write((uint8_t*)"CPK ", 4);
    file.write((uint8_t*)"\xff\0\0\0", 4);
    
    file.seek(0x800 - 6);
    file.write((uint8_t*)"(c)CRI", 6);
    
    //HACK: disable ETOC
    //_cpk_table._columns[_cpk_table.get_column("EtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
    //_cpk_table._columns[_cpk_table.get_column("EtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
    
    //query file sizes
    size_t current_offset = 2048; //be careful: this is *relative*
    size_t current_data_size = 0;
    size_t offset_row = _toc_table.get_column("FileOffset"); //actually columns... TODO: rename
    size_t file_col = _toc_table.get_column("FileName"); 
    size_t dir_col = _toc_table.get_column("DirName"); 
    size_t size_row = _toc_table.get_column("FileSize");
    size_t extractsize_row = _toc_table.get_column("ExtractSize");
    //TODO: assert filetable size == toc table size (and do something if it's not, like resizing toc table)
    for(int i = 0; i < _filetable.size(); i++) {
        while(current_offset % 2048) { current_offset++; }
        size_t old_offset = current_offset;
        _toc_table._rows[i][offset_row].data.i64 = current_offset - 2048;
        _toc_table._rows[i][size_row].data.i64 = _filetable[i].file->size();
        _toc_table._rows[i][extractsize_row].data.i64 = _filetable[i].file->size(); //TODO: fix this
        current_offset += _filetable[i].file->size();
        current_data_size += _filetable[i].file->size();
        
        LOGVER("saved file with size %10d, offset %10d (%016x) and name %s (dir %s)", _filetable[i].file->size(), _toc_table._rows[i][offset_row].data.i64, _toc_table._rows[i][offset_row].data.i64, _toc_table._rows[i][file_col].data.string, _toc_table._rows[i][dir_col].data.string);
        
        file.seek(old_offset);
        _filetable[i].file->seek(0); //reset data file position
        file.write_file(*_filetable[i].file, _filetable[i].file->size());
    }
    
    size_t end_of_data = current_offset;
    LOGVER("total data size (excluding padding): %d", current_data_size);
    _cpk_table.set_by_name<uint64_t>(current_data_size, "EnabledPackedSize", 0);
    _cpk_table.set_by_name<uint64_t>(current_data_size, "EnabledDataSize", 0);
    
    //disable GTOC (since we don't include it)
    _cpk_table._columns[_cpk_table.get_column("GtocOffset")].flags.storage = UTF_Table::Flags::Storage::zero;
    _cpk_table._columns[_cpk_table.get_column("GtocSize")].flags.storage = UTF_Table::Flags::Storage::zero;
    
    _cpk_table.set_by_name<uint32_t>(_toc_table._rows.size(), "Files", 0);
    _cpk_table.set_by_name<uint32_t>(0, "Groups", 0);
    _cpk_table.set_by_name<uint32_t>(0, "Attrs", 0);
    
    _cpk_table.set_by_name<uint16_t>(7, "Version", 0);
    _cpk_table.set_by_name<uint16_t>(2, "Revision", 0);
    
    _cpk_table.set_by_name<uint16_t>(2048, "Align", 0);
    _cpk_table.set_by_name<uint16_t>(1, "Sorted", 0); //TODO: what type of sort is this?
    _cpk_table.set_by_name<uint32_t>(1, "CpkMode", 0);
    
    char* vers_str = (char*)malloc(strlen("N/A, DLL3.11.05") + 1);
    strcpy(vers_str, "N/A, DLL3.11.05");
    _cpk_table.set_by_name<char*>(vers_str, "Tvers", 0);
    
    _cpk_table.set_by_name<uint32_t>(0, "Codec", 0);
    _cpk_table.set_by_name<uint32_t>(0, "DpkItoc", 0);

    
    //save TOC
    file.seek(end_of_data);
    while(file.tell() % 2048) { file.skip(1); }
    _cpk_table.set_by_name<uint64_t>(file.tell() - 2048, "ContentSize", 0);
    _cpk_table.set_by_name<uint64_t>(2048, "ContentOffset", 0);
    {
        LOGVER("saving TOC at position %d", file.tell());
        _cpk_table.set_by_name<uint64_t>(file.tell(), "TocOffset", 0);
        size_t before_toc_packet = file.tell();
        
        
        file.write((uint8_t*)"TOC \xff\0\0\0", 8);
        file.skip(8); //TOC packet size
        
        size_t before_toc = file.tell();
        _toc_table.save(file);
        while(file.tell() % 16) { file.write<uint8_t>('\0'); }
        
        uint64_t toc_size = file.tell() - before_toc;
        
        size_t after_toc = file.tell();
        
        _cpk_table.set_by_name<uint64_t>(file.tell() - before_toc_packet, "TocSize", 0);
        
        file.seek(-toc_size - 8, Tea::Seek_current);
        file.write<uint64_t>(toc_size);
        
        file.seek(after_toc);
    }
    
    //save ETOC
    while(file.tell() % 2048) { file.skip(1); }
    {
        LOGVER("saving ETOC at position %d", file.tell());
        _cpk_table.set_by_name<uint64_t>(file.tell(), "EtocOffset", 0);
        size_t before_etoc_packet = file.tell();
        
        
        file.write((uint8_t*)"ETOC\xff\0\0\0", 8);
        file.skip(8); //TOC packet size
        
        size_t before_etoc = file.tell();
        _etoc_table.save(file);
        while(file.tell() % 16) { file.write<uint8_t>('\0'); }
        uint64_t etoc_size = file.tell() - before_etoc;
        
        _cpk_table.set_by_name<uint64_t>(file.tell() - before_etoc_packet, "EtocSize", 0);
        
        file.seek(-etoc_size - 8, Tea::Seek_current);
        file.write<uint64_t>(etoc_size);
    }
    
    //paste CPK header into file
    file.seek(16);
    LOGVER("saving header at position %d", file.tell());
    _cpk_table.save(file);
    uint64_t cpk_table_size = file.tell() - 16;
    file.seek(8); //move to "table size" portion
    file.write<uint64_t>(cpk_table_size);
    
    return true;
}*/

bool CPK::save() {
    if(!_file)
        return false;
    return this->save(*_file);
}

bool CPK::close() {
    //TODO: finish
    return false;
}

bool CPK::save_directory(std::string& directory) {
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

/*bool CPK::save_directory(std::string& directory) {
    
    //TODO: error stuff on directory etc
    
    fs::create_directories(directory);
    
    
    size_t file_col = _toc_table.get_column("FileName"); 
    size_t dir_col = _toc_table.get_column("DirName"); 
    for(int i = 0; i < _toc_table.num_rows(); i++) {
        fs::path out_path = directory;
        out_path /= _toc_table(i, dir_col)->data.string;
        fs::create_directories(out_path);
        out_path /= _toc_table(i, file_col)->data.string;
        
        Tea::FileDisk f;
        f.open(out_path.u8string().c_str(), Tea::Access_write);
        
        _filetable[i].file->seek(0);
        f.write_file(*_filetable[i].file, _filetable[i].file->size());
        
        f.close();
        
        LOGVER("saved file %s with size %ld", _toc_table(i, file_col)->data.string, _filetable[i].file->size());
    }
    
    return true;
}*/

bool CPK::open_directory(std::string& directory) {
	this->open_empty();
	
	fs::path fs_dir = fs::u8path(directory);
	std::vector<fs::path> files;
	
	std::function<void(fs::path, std::vector<fs::path>&)> sort_dir = [&](fs::path cur_path, std::vector<fs::path>& output) -> void{
		LOGVER("starting sort of %s path (filename %s)", cur_path.u8string().c_str(), cur_path.filename().u8string().c_str());
		LOGBLK
		std::vector<fs::path> local_files;
		for(const auto& p : fs::directory_iterator(cur_path)) { local_files.push_back(p.path()); }
		
		
		std::sort(local_files.begin(), local_files.end(), [](fs::path& a, fs::path& b){ 
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

/*bool CPK::open_directory(std::string& directory) {
    
    this->open_empty();
    
    //TODO: errors on directory etc
    
    size_t col_etoc_dirname = _etoc_table.get_column("LocalDir");
    size_t col_etoc_datetime = _etoc_table.get_column("UpdateDateTime");
    
    size_t col_dirname = _toc_table.get_column("DirName");
    size_t col_filename = _toc_table.get_column("FileName");
    //size_t col_filesize = _toc_table.get_column("FileSize");
    //size_t col_extractsize = _toc_table.get_column("ExtractSize");
    //size_t col_fileoffset = _toc_table.get_column("FileOffset");
    size_t col_id = _toc_table.get_column("ID");
    //size_t col_userstring = _toc_table.get_column("UserString"); //UserString is disabled, so we don't care

    fs::path fs_dir = fs::u8path(directory);
    std::vector<fs::path> files;
    //for(const auto& p : fs::recursive_directory_iterator(directory)) {
    //    if(p.is_regular_file()){
    //        files.push_back(p.path());
    //    }
    //}
    
    //not really required, probably
    //sort(files.begin(), files.end());
    
    std::function<void(fs::path, std::vector<fs::path>&)> sort_dir = [&](fs::path cur_path, std::vector<fs::path>& output) -> void{
        LOGVER("starting sort of %s path (filename %s)", cur_path.u8string().c_str(), cur_path.filename().u8string().c_str());
        LOGBLK
        std::vector<fs::path> local_files;
        for(const auto& p : fs::directory_iterator(cur_path)) { local_files.push_back(p.path()); }
        
        
        std::sort(local_files.begin(), local_files.end(), [](fs::path& a, fs::path& b){ 
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
    
    sort_dir(fs_dir, files);
    
	_toc_table.resize(_toc_table.num_columns(), files.size());
	_etoc_table.resize(_toc_table.num_columns(), files.size());
	
    for(int i = 0; i < files.size(); i++) {
        fs::path& p = files[i];
        fs::path rel_dir_path = fs::relative(p, fs_dir).parent_path();
        
        size_t dir_str_len = strlen(rel_dir_path.u8string().c_str());
        char* dir_str_ptr = (char*)malloc(dir_str_len+1);
        memcpy(dir_str_ptr, rel_dir_path.u8string().c_str(), dir_str_len); dir_str_ptr[dir_str_len] = '\0';
        _toc_table.set<char*>(dir_str_ptr, col_dirname, i);
        _etoc_table.set<char*>(dir_str_ptr, col_etoc_dirname, i); //HACK: double free maybe, just fix leaks everywhere please
        
        size_t name_str_len = strlen(p.filename().u8string().c_str());
        char* name_str_ptr = (char*)malloc(name_str_len+1);
        memcpy(name_str_ptr, p.filename().u8string().c_str(), name_str_len); name_str_ptr[name_str_len] = '\0';
        _toc_table.set<char*>(name_str_ptr, col_filename, i);
        
        _toc_table.set<uint16_t>(i, col_id, i);
        fs::file_time_type mod_time = fs::last_write_time(p);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(mod_time - fs::file_time_type::clock::now()
              + std::chrono::system_clock::now());
    
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm *gmt = std::gmtime(&tt);
        
        //TODO: time should be inverted or something? idk, fix it. or just abandon time alltogether.
        uint64_t cool_time = ((uint64_t)0x07 << 56) | (((uint64_t)(gmt->tm_year + 1900) & 0xFFFF) << 40) | ((uint64_t)gmt->tm_mday << 32) | ((uint64_t)gmt->tm_mon << 24) | ((uint64_t)gmt->tm_hour << 16) | ((uint64_t)gmt->tm_min << 8) | ((uint64_t)gmt->tm_sec << 0);
        
        _etoc_table.set<uint64_t>(cool_time, col_etoc_datetime, i);
        
        //LOGVER("time: y=%d, m=%d, d=%d", gmt->tm_year + 1900, gmt->tm_mon, gmt->tm_mday);
        
        LOGVER("loading file %s", p.u8string().c_str());

        
        //all other entries (size etc) are set when saving
        Tea::FileDisk nfile;
        nfile.open(p.u8string().c_str(), Tea::Access_read);
        
        Tea::FileMemory* memf = new Tea::FileMemory(); //we need to use a memory file, cuz we run out of file descriptors otherwise
        memf->open_owned();
        memf->write_file(nfile, nfile.size());
        
        nfile.close();
        
        Entry ent;
        ent.file = memf;
        ent.filename = name_str_ptr;
        ent.dirname = dir_str_ptr;
        _filetable.push_back(ent);
    }
    
    _etoc_table.resize(_etoc_table.num_columns(), _etoc_table.num_rows()+1);
    _etoc_table.set<uint64_t>(0, col_etoc_datetime, _etoc_table.num_rows() - 1);
    _etoc_table.set<char*>("", col_etoc_dirname, _etoc_table.num_rows() - 1);

    
    return true;
}*/

bool CPK::open_empty(Tea::File& file) {
    this->close();
    _file = &file;
    return this->open_empty();
}

bool CPK::open_empty() {
	return true;
} //HACK: to make it compile

/*bool CPK::open_empty() {
    //TODO: initialize utf tables with default columns
    
    _cpk_table._table_name = "CpkHeader";
    _toc_table._table_name = "CpkTocInfo";
    _etoc_table._table_name = "CpkEtocInfo";
    
    //default initialization values
#define TF(type, storage, string) UTF_Table::Column(UTF_Table::Flags(UTF_Table::Flags::Type::type, UTF_Table::Flags::Storage::storage), string)
#define CE(type, storage, string) _cpk_table._columns.push_back(TF(type, storage, string))
    _cpk_table._columns.resize(0);
    CE(i64, per_row, "UpdateDateTime");
    CE(i64, zero, "FileSize");
    CE(i64, per_row, "ContentOffset");
    CE(i64, per_row, "ContentSize");
    CE(i64, per_row, "TocOffset");
    CE(i64, per_row, "TocSize");
    CE(i32, zero, "TocCrc");
    CE(i64, per_row, "EtocOffset");
    CE(i64, per_row, "EtocSize");
    CE(i64, zero, "ItocOffset");
    CE(i64, zero, "ItocSize");
    CE(i32, zero, "ItocCrc");
    CE(i64, zero, "GtocOffset");
    CE(i64, zero, "GtocSize");
    CE(i32, zero, "GtocCrc");
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
    CE(i16, zero, "EID");
    CE(i32, per_row, "CpkMode");
    CE(str, per_row, "Tvers");
    CE(str, zero, "Comment");
    CE(i32, per_row, "Codec");
    CE(i32, per_row, "DpkItoc");
	_cpk_table.resize(_cpk_table.num_columns(), 1);
#undef CE
#define TE(type, storage, string) _toc_table._columns.push_back(TF(type, storage, string))
    _toc_table._columns.resize(0);
    TE(str, per_row, "DirName");
    TE(str, per_row, "FileName");
    TE(i32, per_row, "FileSize");
    TE(i32, per_row, "ExtractSize");
    TE(i64, per_row, "FileOffset");
    TE(i32, per_row, "ID");
    TE(str, constant, "UserString");
#undef TE
#define EE(type, storage, string) _etoc_table._columns.push_back(TF(type, storage, string))
    _etoc_table._columns.resize(0);
    EE(i64, per_row, "UpdateDateTime");
    EE(str, per_row, "LocalDir");
#undef TF
    
    
    _cpk_table.set_by_name<uint64_t>(1, "UpdateDateTime", 0);
    
    return true;
}*/

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
		
		LOGWAR("not handling DataL yet, only DataH"); //TODO: DataL
		
		uint32_t content_processed = 0;
		for(size_t i = 0; i < u_data_h.num_rows(); i++) {
			Entry ent;
			uint32_t ID;
			uint32_t size;
			uint32_t extract_size;
			bool ok = true;
			ok &= u_data_h.get_by_name(ID, "ID", i);
			ok &= u_data_h.get_by_name(size, "FileSize", i);
			ok &= u_data_h.get_by_name(extract_size, "ExtractSize", i);
			ent.ID = ID;
			ent.is_id_based = true;
			
			Tea::FileMemory* mem = new Tea::FileMemory();
			mem->open_owned();
			
			_file->seek(content_offset + content_processed);
			char cri_check[9] = "notcrill";
			if(size >= 16) {
				_file->read((uint8_t*)cri_check, 8);
				_file->seek(-8, Tea::Seek_current);
			}
			
			if(size != extract_size && !memcmp(cri_check, "CRILAYLA", 8)) {
				decompress_crilayla(*_file, size, *mem);
			}
			else {
				mem->write_file(*_file, size);
			}
			ent.file = mem;
			
			processed_files++;
			processed_bytes += extract_size;
			
			content_processed += size;
			LOGINF("new thingy size %d (offset is now %d)", extract_size, content_offset + content_processed);
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

//TODO: those fancy file abstractions are cool and all, but for the love of god just integrate readUTF and decryptUTF with the UTF_table class. reading this code is painful
/*bool CPK::open(Tea::File& file) {
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
    _cpk_table.open(cpk_file_packet);
    
	bool maybe_id_file = false; //if there's no TOC, the file could be entirely ID-based (as seen in ULJM05752)
	
	{ //read TOC and open files
        int toc_offset_column = _cpk_table.get_column("TocOffset");
        int toc_size_column = _cpk_table.get_column("TocSize");

        if(toc_offset_column == -1 || toc_size_column == -1) { LOGERR("TocOffset or TocSize not found in CPK header"); return false; }
        uint64_t toc_offset = -1;
        if(!_cpk_table.get(toc_offset, toc_offset_column, 0)) {
			LOGINF("TocOffset could not be read from table, could be an ID-based file");
			maybe_id_file = true;
			goto after_toc;
		}
        _file->seek(toc_offset);
		LOGVER("toc offset: %d", toc_offset);
        
        if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading TOC magic"); return false; }
        if(strncmp(magic, "TOC ", 4)) { LOGERR("expected TOC magic, but didn't get it"); return false; }
        std::vector<uint8_t> toc_packet;
        readUTF(*_file, toc_packet);
        Tea::FileMemory toc_file_packet(*toc_packet.data(), toc_packet.size());
        _toc_table.open(toc_file_packet);
        
        _filetable.resize(_toc_table.num_rows());
        for(int i = 0; i < _toc_table.num_rows(); i++) {
            char* tmp_ptr = nullptr;
            _toc_table.get_by_name(tmp_ptr, "DirName", i); if(tmp_ptr) { _filetable[i].dirname = tmp_ptr; }
            _toc_table.get_by_name(tmp_ptr, "FileName", i); if(tmp_ptr) { _filetable[i].filename = tmp_ptr; }
            _toc_table.get_by_name(_filetable[i].filesize, "FileSize", i);
            _toc_table.get_by_name(_filetable[i].extractsize, "ExtractSize", i);
            uint32_t file_offset = -1;
            _toc_table.get_by_name(file_offset, "FileOffset", i);
            _toc_table.get_by_name(tmp_ptr, "UserString", i); if(tmp_ptr) { _filetable[i].userstring = tmp_ptr; }
            _toc_table.get_by_name(_filetable[i].ID, "ID", i);
            
            if(file_offset == -1)
                LOGERR("offset of file %s (%d) is invalid", _filetable[i].filename.c_str(), i);
            
            LOGVER("size %010d, extractsize %010d, dir %s, name %s", _filetable[i].filesize, _filetable[i].extractsize, _filetable[i].dirname.c_str(), _filetable[i].filename.c_str());

            Tea::FileMemory* mem = new Tea::FileMemory();
            mem->open_owned();
            _file->seek(file_offset + 2048);
            char cri_check[8] = "notcri";
            if(_filetable[i].filesize >= 16) {
                _file->read((uint8_t*)cri_check, 8);
                _file->seek(-8, Tea::Seek_current);
            }
            //TODO: also use extractsize vs filesize to check for compression
            if(!memcmp(cri_check, "CRILAYLA", 8)) { //check if file is CRILAYLA compressed
                decompress_crilayla(*_file, _filetable[i].filesize, *mem);
                _filetable[i].extractsize = _filetable[i].filesize;
            }
            else {
                mem->write_file(*_file, _filetable[i].filesize);
            }
            
            
            _filetable[i].file = mem;
            
			processed_files++;
			processed_bytes += _filetable[i].filesize;
        }
    }
after_toc:
    
    if(!maybe_id_file) { //read ETOC
        int etoc_offset_column = _cpk_table.get_column("EtocOffset");
        int etoc_size_column = _cpk_table.get_column("EtocSize");
        if(etoc_offset_column == -1 || etoc_size_column == -1) { LOGERR("EtocOffset or EtocSize not found in CPK header"); return false; }
        uint64_t etoc_offset;
        _cpk_table.get(etoc_offset, etoc_offset_column, 0);
        _file->seek(etoc_offset);
        
        if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading ETOC magic"); return false; }
        if(strncmp(magic, "ETOC", 4)) { LOGERR("expected ETOC magic, but didn't get it"); return false; }
        std::vector<uint8_t> etoc_packet;
        readUTF(*_file, etoc_packet);
        Tea::FileMemory etoc_file_packet(*etoc_packet.data(), etoc_packet.size());
        _etoc_table.open(etoc_file_packet);
        
        if(_filetable.size() != _etoc_table.num_rows()) {
			//TODO: is this bad?
            //LOGWAR("ETOC table doesn't contain the same amount of entries as the TOC table, which might be quite bad (yes, I don't know if it is) (%d vs %d)", _filetable.size(), _etoc_table._rows.size());
        }
        
        for(int i = 0; i < std::min((int)_filetable.size(), _etoc_table.num_rows()); i++) {
            char* tmp_ptr = nullptr;
            _etoc_table.get_by_name(tmp_ptr, "LocalDir", i); if(tmp_ptr) { _filetable[i].localdir = tmp_ptr; }
            _etoc_table.get_by_name(_filetable[i].updatedatetime, "UpdateDateTime", i);
            LOGVER("file %d has local dir of %s", i, _filetable[i].localdir.c_str());
        }
    }
    
	{ //read ITOC
        int itoc_offset_column = _cpk_table.get_column("ItocOffset");
        int itoc_size_column = _cpk_table.get_column("ItocSize");
        if(itoc_offset_column == -1 || itoc_size_column == -1) { LOGERR("ItocOffset or ItocSize not found in CPK header"); return false; }
        uint64_t itoc_offset;
        bool got_offset_correct = _cpk_table.get(itoc_offset, itoc_offset_column, 0);
        _file->seek(itoc_offset);
        
        if(!got_offset_correct) {
            LOGVER("no ITOC in this file");
			if(maybe_id_file) {
				LOGERR("no ITOC is bad, since there's no TOC either. no clue what to do with this file!");
				return false;
			}
            goto after_itoc;
        }
        
        if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading ITOC magic"); return false; }
        if(strncmp(magic, "ITOC", 4)) { LOGERR("expected ITOC magic, but didn't get it"); return false; }
        std::vector<uint8_t> itoc_packet;
        readUTF(*_file, itoc_packet);
        UTF_Table itoc_table;
        Tea::FileMemory itoc_file_packet(*itoc_packet.data(), itoc_packet.size());
        itoc_table.open(itoc_file_packet);
        
		UTF_Table::Unit::Data::Data_type data_l;
		UTF_Table::Unit::Data::Data_type data_h;
		itoc_table.get_by_name(data_l, "DataL", 0);
		itoc_table.get_by_name(data_h, "DataH", 0);
		Tea::FileMemory f_data_l;
		Tea::FileMemory f_data_h;
		f_data_l.open(data_l.data, data_l.len);
		f_data_h.open(data_h.data, data_h.len);
		UTF_Table u_data_l;
		UTF_Table u_data_h;
		u_data_l.open(f_data_l);
		u_data_h.open(f_data_h);
		
		_filetable.resize(u_data_h.num_rows());
		_toc_table.resize(_toc_table.num_columns(), u_data_h.num_rows());
		//uint32_t curr_offs = 
		for(int i = 0; i < u_data_h.num_rows(); i++) {
			
			//ID, FileSize, ExtractSize
			char* tmp_ptr = (char*)malloc(256);
			uint32_t curr_id = -1;
			bool id_ok = u_data_h.get_by_name(curr_id, "ID", i);
			snprintf(tmp_ptr, 256, "__ID_%04d", curr_id);
			_filetable[i].filename = tmp_ptr;
			_filetable[i].dirname = "";
			
			uint32_t curr_size;
			uint32_t curr_extract_size;
			u_data_h.get_by_name(curr_size, "FileSize", i);
			u_data_h.get_by_name(curr_extract_size, "ExtractSize", i);
			
			Tea::FileMemory* mem = new Tea::FileMemory();
			mem->open_owned();
			//_file->seek(file_offset + 2048);
			char cri_check[8] = "notcri";
			if(_filetable[i].filesize >= 16) {
				_file->read((uint8_t*)cri_check, 8);
				_file->seek(-8, Tea::Seek_current);
			}
			//TODO: also use extractsize vs filesize to check for compression
			if(!memcmp(cri_check, "CRILAYLA", 8)) { //check if file is CRILAYLA compressed
				decompress_crilayla(*_file, _filetable[i].filesize, *mem);
				_filetable[i].extractsize = _filetable[i].filesize;
			}
			else {
				mem->write_file(*_file, _filetable[i].filesize);
			}
			
		}
		//for(int i = 0; i < itoc_table._num_rows; i++) {
		//	char* tmp_ptr = nullptr;
		//	_toc_table.get_by_name(tmp_ptr, "DirName", i); if(tmp_ptr) { _filetable[i].dirname = tmp_ptr; }
		//	_toc_table.get_by_name(tmp_ptr, "FileName", i); if(tmp_ptr) { _filetable[i].filename = tmp_ptr; }
		//	_toc_table.get_by_name(_filetable[i].filesize, "FileSize", i);
		//	_toc_table.get_by_name(_filetable[i].extractsize, "ExtractSize", i);
		//	uint32_t file_offset = -1;
		//	_toc_table.get_by_name(file_offset, "FileOffset", i);
		//	_toc_table.get_by_name(tmp_ptr, "UserString", i); if(tmp_ptr) { _filetable[i].userstring = tmp_ptr; }
		//	_toc_table.get_by_name(_filetable[i].ID, "ID", i);
		//	
		//	if(file_offset == -1)
		//		LOGERR("offset of file %s (%d) is invalid", _filetable[i].filename.c_str(), i);
		//	
		//	LOGVER("size %010d, extractsize %010d, dir %s, name %s", _filetable[i].filesize, _filetable[i].extractsize, _filetable[i].dirname.c_str(), _filetable[i].filename.c_str());
		//	
		//	Tea::FileMemory* mem = new Tea::FileMemory();
		//	mem->open_owned();
		//	_file->seek(file_offset + 2048);
		//	char cri_check[8] = "notcri";
		//	if(_filetable[i].filesize >= 16) {
		//		_file->read((uint8_t*)cri_check, 8);
		//		_file->seek(-8, Tea::Seek_current);
		//	}
		//	//TODO: also use extractsize vs filesize to check for compression
		//	if(!memcmp(cri_check, "CRILAYLA", 8)) { //check if file is CRILAYLA compressed
		//		decompress_crilayla(*_file, _filetable[i].filesize, *mem);
		//		_filetable[i].extractsize = _filetable[i].filesize;
		//	}
		//	else {
		//		mem->write_file(*_file, _filetable[i].filesize);
		//	}
		//	
		//	
		//	_filetable[i].file = mem;
		//	
		//	processed_files++;
		//	processed_bytes += _filetable[i].filesize;
		//}
		
        LOGERR("NOT IMPLEMENTED: ITOC reading");
    }
after_itoc:
    
    { //read GTOC
        //HACK we ignore GTOC for now, fix this maybe
        LOGVER("ignoring possible GTOC section");
        goto after_gtoc;
        
        int gtoc_offset_column = _cpk_table.get_column("GtocOffset");
        int gtoc_size_column = _cpk_table.get_column("GtocSize");
        if(gtoc_offset_column == -1 || gtoc_size_column == -1) { LOGERR("GtocOffset or GtocSize not found in CPK header"); return false; }
        uint64_t gtoc_offset;
        bool got_offset_correct = _cpk_table.get(gtoc_offset, gtoc_offset_column, 0);
        _file->seek(gtoc_offset);
        
        if(!got_offset_correct) {
            LOGINF("no GTOC in this file");
        }
        
        if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading GTOC magic"); return false; }
        if(strncmp(magic, "GTOC", 4)) { LOGERR("expected GTOC magic, but didn't get it"); return false; }
        
        _gtoc_data.resize(gtoc_size_column);
        _file->read(_gtoc_data.data(), _gtoc_data.size());
        
        //std::vector<uint8_t> gtoc_packet;
        //readUTF(*_file, gtoc_packet);
        //UTF_Table _gtoc_table;
        //Tea::FileMemory gtoc_file_packet(*gtoc_packet.data(), gtoc_packet.size());
        //_gtoc_table.open(gtoc_file_packet);
        
        LOGWAR("not properly implemented: GTOC reading");
    }
after_gtoc:
    
    LOGOK("processed %d files with a combined size of %d bytes", processed_files, processed_bytes);
    return true;
}*/


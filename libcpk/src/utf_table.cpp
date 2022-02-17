#include "cpk.hpp"





void UTF_Table::save(Tea::File& file) {
	Tea::Endian old_endian = file.endian();
	file.write((uint8_t*)"@UTF", 4);
	size_t start_offset = file.tell() + 4;
	file.endian(Tea::Endian::big);
	file.skip(4 * 4); //skip all offsets
	
	//Tea::FileMemory strings;
	//strings.open_owned();
	//strings.write_c_string("<NULL>");
	//file.write<uint32_t>((uint32_t)strings.tell()); //offset into string table
	//strings.write_c_string(_table_name.c_str(), 1707); //not an actual limit
	
	std::vector<std::string> strings;
	
	
	//returns offset of new string
	auto save_new_string = [&](std::string str) -> uint32_t {
		uint32_t offs = 0;
		for(const auto& a : strings) {
			if(a == str) {
				//LOGVER("offs=%d, found repeated string %s", offs, str.c_str());
				return offs;
			}
			offs += a.length() + 1;
		}
		
		strings.push_back(str);
		//LOGVER("offs=%d, added new string %s", offs, str.c_str());
		return offs;
	};
	save_new_string("<NULL>");
	file.write<uint32_t>(save_new_string(_table_name));
	
	std::vector<uint8_t> data;
	//returns offset of new data
	auto save_new_data = [&](Unit::Data::Data_type new_dat) -> uint32_t {
		uint32_t ret = data.size();
		for(int i = 0; i < new_dat.len; i++) {
			data.push_back(new_dat.data[i]); //TODO: make less horrid
		}
		return ret;
	};
	
	uint16_t num_col = this->num_columns();
	file.write(num_col);
	uint16_t row_length = 0;
	
	for(int i = 0; i < num_col; i++) {
		Flags::Storage storage_flag = _columns[i].flags.storage;
		if(storage_flag == Flags::Storage::none
			|| storage_flag == Flags::Storage::zero
			|| storage_flag == Flags::Storage::constant) {
			
			continue;
			}
			
			switch(_columns[i].flags.type) {
				default:
					LOGERR("invalid type value (%02x)\n", (int)_columns[i].flags.type);
					break;
				case Flags::Type::i8:
				case Flags::Type::i8t:
					row_length += 1;
					break;
				case Flags::Type::i16:
				case Flags::Type::i16t:
					row_length += 2;
					break;
				case Flags::Type::str:
				case Flags::Type::f32:
				case Flags::Type::i32:
				case Flags::Type::i32t:
					row_length += 4;
					break;
				case Flags::Type::data:
				case Flags::Type::i64:
				case Flags::Type::i64t:
					row_length += 8;
					break;
			}
	}
	file.write(row_length);
	uint32_t num_row = num_rows();
	file.write(num_row);
	
	//save all columns
	for(int i = 0; i < num_columns(); i++) {
		file.write<uint8_t>(*(uint8_t*)&_columns[i].flags);
		file.write<uint32_t>(save_new_string(_columns[i].name.c_str()));
		if(_columns[i].flags.storage == Flags::Storage::constant) {
			file.skip(4); //fix later (see declaration of Column struct)
		}
	}
	
	uint32_t rows_offset = file.tell() - start_offset;
	
	//save all rows
	for(int r = 0; r < num_rows(); r++) {
		//if(_rows[r].size() != _columns.size())
		//    LOGERR("not enough data in row %d, was expecting %d (is actually %d)", y, _columns.size(), _rows[y].size());
		
		for(int c = 0; c < num_columns(); c++) {
			Flags::Storage storage_flag = _columns[c].flags.storage;
			if(storage_flag == Flags::Storage::none
				|| storage_flag == Flags::Storage::zero
				|| storage_flag == Flags::Storage::constant) {
				
				continue;
				}
				
				switch(_columns[c].flags.type) {
					default:
						LOGERR("invalid type value (%02x)\n", (int)_columns[c].flags.type);
						break;
					case Flags::Type::i8:
					case Flags::Type::i8t:
						file.write(_rows[c][r].data.i8);
						break;
					case Flags::Type::i16:
					case Flags::Type::i16t:
						file.write(_rows[c][r].data.i16);
						break;
					case Flags::Type::f32:
						file.write(_rows[c][r].data.flt);
						break;
					case Flags::Type::i32:
					case Flags::Type::i32t:
						file.write(_rows[c][r].data.i32);
						break;
					case Flags::Type::i64:
					case Flags::Type::i64t:
						file.write(_rows[c][r].data.i64);
						break;
					case Flags::Type::str:
						if(!_rows[c][r].data.string) {
							file.write<uint32_t>(0);
							break;
						}
						
						file.write<uint32_t>(save_new_string(_rows[c][r].data.string));
						break;
					case Flags::Type::data:
						if(!_rows[c][r].data.data.data || _rows[c][r].data.data.len == 0){
							file.write<uint32_t>(0);
							file.write<uint32_t>(0);
							break;
						}
						file.write<uint32_t>(save_new_data(_rows[c][r].data.data));
						file.write<uint32_t>(_rows[c][r].data.data.len);
						break;
				}
		}
	}
	
	uint32_t strings_offset = file.tell() - start_offset;
	LOGVER("strings offset: %d", strings_offset);
	for(int i = 0; i < strings.size(); i++) {
		file.write((uint8_t*)strings[i].data(), strings[i].length());
		file.write('\0');
	}
	
	while(file.tell() % 8) { file.skip(1); } //HACK: this might be bogus
	uint32_t data_offset = file.tell() - start_offset;
	LOGVER("data offset: %d", data_offset);
	file.write(data.data(), data.size());
	
	uint32_t table_size = file.tell() - start_offset;
	size_t finish_offset = file.tell();
	LOGVER("table size: %d", table_size);
	
	file.seek(start_offset - 4);
	file.write(table_size);
	file.write(rows_offset);
	file.write(strings_offset);
	file.write(data_offset);
	
	file.seek(finish_offset);
	
	file.endian(old_endian);
}

void UTF_Table::open(Tea::File& file) {
	Tea::Endian old_endian = file.endian();
	
	if(false) {
		Tea::FileDisk d;
		d.open(std::to_string(rand()).c_str(), Tea::Access_write);
		size_t old_offs = file.tell();
		d.write_file(file, file.size());
		file.seek(old_offs);
	}
	
	size_t offset = file.tell();
	uint32_t magic = file.read<uint32_t>();
	
	file.endian(Tea::Endian::big);
	file.read(_table_size);
	_rows_offset = file.read<uint32_t>() + offset + 8;
	_strings_offset = file.read<uint32_t>() + offset + 8;
	_data_offset = file.read<uint32_t>() + offset + 8;
	
	uint32_t table_name_offset;
	file.read(table_name_offset);
	uint16_t new_num_col;
	uint32_t new_num_row;
	file.read(new_num_col);
	file.read(_row_length);
	file.read(new_num_row);
	
	resize(new_num_col, new_num_row);
	for(int i = 0; i < _columns.size(); i++) {
		file.read(_columns[i].flags);
		uint32_t name_offset = file.read<uint32_t>();
		size_t offs = file.tell();
		file.seek(name_offset + _strings_offset);
		while(char curr = file.read<char>()) {
			_columns[i].name += curr;
		}
		file.seek(offs);
		
		if(_columns[i].flags.storage == Flags::Storage::constant) {
			file.skip(4);
			LOGERR("unimplemented 'Flags::Storage::constant'");
		}
	}
	
	
	file.seek(table_name_offset + _strings_offset);
	while(char curr = file.read<char>()) {
		_table_name += curr;
	}
	
	LOGVER("loaded table '%s' with %d columns:", _table_name.c_str(), num_columns());
	for(int i = 0; i < num_columns(); i++) {
		LOGVER(" %02d  %s", i, _columns[i].name.c_str());
	}
	
	
	for(int r = 0; r < num_rows(); r++) {
		file.seek(_rows_offset + (r * _row_length));
		
		for(int c = 0; c < num_columns(); c++) {
			
			Flags::Storage storage_flag = _columns[c].flags.storage;
			if(storage_flag == Flags::Storage::none
				|| storage_flag == Flags::Storage::zero
				|| storage_flag == Flags::Storage::constant) {
					continue;
				}
				
			//HACK: what about Storage::constant?
			
			switch(_columns[c].flags.type) {
				default:
					LOGERR("invalid type value (%02x)\n", (int)_columns[c].flags.type);
					break;
				case Flags::Type::i8:
				case Flags::Type::i8t:
					file.read(_rows[c][r].data.i8);
					break;
				case Flags::Type::i16:
				case Flags::Type::i16t:
					file.read(_rows[c][r].data.i16);
					break;
				case Flags::Type::i32:
				case Flags::Type::i32t:
					file.read(_rows[c][r].data.i32);
					break;
				case Flags::Type::i64:
				case Flags::Type::i64t:
					file.read(_rows[c][r].data.i64);
					break;
				case Flags::Type::f32:
					file.read(_rows[c][r].data.flt);
					break;
				case Flags::Type::str: {
					uint32_t tmp_offset = file.tell();
					file.seek(file.read<uint32_t>() + _strings_offset);
					std::string str;
					//ugly
					while(char curr = file.read<char>()) {
						str += curr;
					}
					size_t str_len = str.length();
					_rows[c][r].data.string = (char*)malloc(str_len+1);
					memcpy(_rows[c][r].data.string, str.c_str(), str_len);
					_rows[c][r].data.string[str_len] = '\0';
					file.seek(tmp_offset + 4);
					break;
				}
				case Flags::Type::data: {
					uint32_t offset;
					file.read(offset);
					offset += _data_offset;
					
					uint32_t size;
					file.read(size);
					
					uint32_t tmp_offset = file.tell();
					file.seek(offset);
					uint8_t* new_data = (uint8_t*)malloc(size);
					file.read(new_data, size);
					file.seek(tmp_offset);
					
					_rows[c][r].data.data.data = new_data;
					_rows[c][r].data.data.len = size;
					break;
				}
			}
		}
	}
	
	file.endian(old_endian);
}

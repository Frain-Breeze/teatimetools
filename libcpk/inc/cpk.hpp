#pragma once

#include <teaio_file.hpp>
#include <tea_pack.hpp>

#include <vector>
#include <string>
#include <memory.h>
#include <typeinfo>

class UTF_Table {
public:

    TEA_PACK_WIN_START;
    struct Flags {
        enum class Storage : uint8_t {
            none = 0,
            zero = 1,
            constant = 3,
            per_row = 5,
        };

        enum class Type : uint8_t {
            i8 = 0,
            i8t = 1,
            i16 = 2,
            i16t = 3,
            i32 = 4,
            i32t = 5,
            i64 = 6,
            i64t = 7,
            f32 = 8,
            str = 0xA,
            data = 0xB,
        };
        
        Type type : 4;
        Storage storage : 4;

        Flags() {}
        Flags(Type _type, Storage _storage) : type(_type), storage(_storage) {}
    } TEA_PACK_LIN;
    TEA_PACK_WIN_END;
    static_assert(sizeof(Flags) == 1);

    uint32_t _table_size;
    uint32_t _rows_offset;
    uint32_t _strings_offset;
    uint32_t _data_offset;

    std::string _table_name;
    uint16_t _row_length;
private:
    //uint16_t _num_columns;
    //uint32_t _num_rows;
public:
	
    struct Unit {
        union Data {
            uint8_t i8;
            uint16_t i16;
            uint32_t i32;
            uint64_t i64;
            float flt;
            char* string;
            struct Data_type {
				uint32_t len;
				uint8_t* data;
			} data;
        } data;
        
        //Flags::Type type;
    };
    
    struct Column {
        Flags flags;
        std::string name;
        Unit::Data constant_data; //TODO: read/write this (currently just assuming it's a string pointing to 0)

        Column() {}
        Column(Flags _flags, std::string _name) : flags(_flags), name(_name) {}
    };

    
	Unit* operator()(int column, int row) {
		if(column < 0 || column > num_columns() || row < 0 || row > num_rows()) { return nullptr; }
		return &_rows[column][row];
	}
	Unit* operator()(std::string column, int row) {
		if(row < 0 || row > num_rows()) { return nullptr; }
		
		int col_index = -1;
		for(int i = 0; i < _columns.size(); i++) {
			if(_columns[i].name == column) {
				col_index = i;
				break;
			}
		}
		if(col_index == -1) { return nullptr; }
		return &_rows[col_index][row];
	}
	
	void resize(int col_count, int row_count) {
		_columns.resize(col_count);
		_rows.resize(col_count);
		for(int i = 0; i < col_count; i++) {
			_rows[i].resize(row_count);
		}
		private_real_row_count = row_count;
	}
	
	int num_rows() { return private_real_row_count; }
	int num_columns() { return _columns.size(); }
	
    std::vector<Column> _columns; //only contains info on types and names
private:
    std::vector<std::vector<Unit>> _rows; //contains actual data, ordered like _rows[column][row]
    int private_real_row_count = 0;
public:
    
    template<typename T>
    bool get(T& type, size_t column, size_t row) {
        //TODO: check if type matches with value in this column
        if(column >= num_columns() || row >= num_rows()) { return false; }
        if(_columns[column].flags.storage == Flags::Storage::zero) { return false; } //HACK: is this correct?
        
        if(typeid(T) == typeid(Unit::Data::Data_type)) {
			type = *(T*)&_rows[column][row].data.data;
		}
        else if(sizeof(T) == 8) {
            type = *(T*)&_rows[column][row].data.i64;
        }
        else if(sizeof(T) == 4) {
            type = *(T*)&_rows[column][row].data.i32;
        }
        
        return true;
    }
    
    template<typename T>
    bool set(T type, size_t column, size_t row) {
        //TODO: check if type matches with value in this column
        if(column >= num_columns() || row >= num_rows()) { return false; }
        if(_columns[column].flags.storage == Flags::Storage::zero) { return false; } //HACK: is this correct?
        
        //why am I making it this ugly again?
        if(typeid(T) == typeid(Unit::Data::Data_type)) {
			*(T*)&_rows[column][row].data.data = type;
		}
        else if(sizeof(T) == 8) {
            *(T*)&_rows[column][row].data.i64 = type;
        }
        else if(sizeof(T) == 4) {
            *(T*)&_rows[column][row].data.i32 = type;
        }
        else if(sizeof(T) == 2) {
            *(T*)&_rows[column][row].data.i16 = type;
        }
        else if(sizeof(T) == 1) {
            *(T*)&_rows[column][row].data.i8 = type;
        }
        else {
            LOGERR("very bad thingy");
            throw "issue";
        }
        
        return true;
    }
    
    int get_column(std::string name) {
        for(int i = 0; i < _columns.size(); i++) {
            if(_columns[i].name == name) {
                return i;
            }
        }
        return -1;
    }
    
    template<typename T>
    bool get_by_name(T& type, std::string name, int row) {
        int col = get_column(name);
        if(col == -1) { return false; }
        if(row > num_rows()) { return false; }
        return get(type, col, row);
    }
    
    template<typename T>
    bool set_by_name(T type, std::string name, int row) {
        int col = get_column(name);
        if(col == -1) { return false; }
        if(row > num_rows()) { return false; }
        return set(type, col, row);
    }
    
    //...
    
    ~UTF_Table() {
        //TODO: don't forget to deallocate the strings in the table
        LOGWAR("NOT IMPLEMENTED");
    }

    void save(Tea::File& file) {
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
    
    void open(Tea::File& file) {
        Tea::Endian old_endian = file.endian();
        
		if(true) { //HACK: debug thing
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
                    case Flags::Type::data:
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
        
        file.endian(old_endian);
    }
};

class CPK {
public:
    bool open(Tea::File& file);
    bool open_empty(Tea::File& file);
    bool open_empty();
    bool close();
    bool save(); //save to original file
    bool save(Tea::File& file); //save to user supplied file
    
    bool open_directory(const std::string& directory);
    bool save_directory(const std::string& directory);

    struct Entry {
        //data:
        Tea::File* file = nullptr;
        
        //TOC: (partially also ITOC)
        std::string dirname;
        std::string filename;
        uint32_t ID;
		std::string userstring;
        
		bool is_id_based = false;
		
        //ETOC:
        std::string localdir;
        uint64_t updatedatetime;
    };
    std::vector<Entry> _filetable;
    
private:
    Tea::File* _file = nullptr;
};

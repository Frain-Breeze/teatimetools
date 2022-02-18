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
		
		Flags() { type = Type::i8; storage = Storage::none; }
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

	~UTF_Table();
	
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
	
	void save(Tea::File& file);
	void open(Tea::File& file);
};

class CPK {
public:
	~CPK();
	
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

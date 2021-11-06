#pragma once

#include <teaio_file.hpp>
#include <tea_pack.hpp>

#include <vector>
#include <string>
#include <memory.h>

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
    uint16_t _num_columns;
    uint16_t _row_length;
    uint32_t _num_rows;

    struct Unit {
        union Data {
            uint8_t i8;
            uint16_t i16;
            uint32_t i32;
            uint64_t i64;
            float flt;
            char* string;
            //data type 0xB?
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

    

    std::vector<Column> _columns; //only contains info on types and names
    std::vector<std::vector<Unit>> _rows; //contains actual data
    
    template<typename T>
    bool get(T& type, size_t column, size_t row) {
        //TODO: check if type matches with value in this column
        if(column >= _columns.size() || row >= _rows.size()) { return false; }
        if(_columns[column].flags.storage == Flags::Storage::zero) { return false; } //HACK: is this correct?
        
        if(sizeof(T) == 8) {
            type = *(T*)&_rows[row][column].data.i64;
        }
        else if(sizeof(T) == 4) {
            type = *(T*)&_rows[row][column].data.i32;
        }
        
        return true;
    }
    
    template<typename T>
    bool set(T type, size_t column, size_t row) {
        //TODO: check if type matches with value in this column
        if(column >= _columns.size() || row >= _rows.size()) { return false; }
        if(_columns[column].flags.storage == Flags::Storage::zero) { return false; } //HACK: is this correct?
        
        //why am I making it this ugly again?
        if(sizeof(T) == 8) {
            *(T*)&_rows[row][column].data.i64 = type;
        }
        else if(sizeof(T) == 4) {
            *(T*)&_rows[row][column].data.i32 = type;
        }
        else if(sizeof(T) == 2) {
            *(T*)&_rows[row][column].data.i16 = type;
        }
        else if(sizeof(T) == 1) {
            *(T*)&_rows[row][column].data.i8 = type;
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
        if(row > _rows.size()) { return false; }
        return get(type, col, row);
    }
    
    template<typename T>
    bool set_by_name(T type, std::string name, int row) {
        int col = get_column(name);
        if(col == -1) { return false; }
        if(row > _rows.size()) { return false; }
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
        
        
        uint16_t num_col = _columns.size();
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
                case Flags::Type::i64:
                case Flags::Type::i64t:
                    row_length += 8;
                    break;
                case Flags::Type::data:
                    LOGERR("data not implemented");
                    break;
            }
        }
        file.write(row_length);
        uint32_t num_row = _rows.size();
        file.write(num_row);
        
        //save all columns
        for(int i = 0; i < _columns.size(); i++) {
            file.write<uint8_t>(*(uint8_t*)&_columns[i].flags);
            file.write<uint32_t>(save_new_string(_columns[i].name.c_str()));
            if(_columns[i].flags.storage == Flags::Storage::constant) {
                file.skip(4); //fix later (see declaration of Column struct)
            }
        }
        
        uint32_t rows_offset = file.tell() - start_offset;
        
        //save all rows
        for(int y = 0; y < _rows.size(); y++) {
            if(_rows[y].size() != _columns.size())
                LOGERR("not enough data in row %d, was expecting %d (is actually %d)", y, _columns.size(), _rows[y].size());
            
            for(int x = 0; x < _columns.size(); x++) {
                Flags::Storage storage_flag = _columns[x].flags.storage;
                if(storage_flag == Flags::Storage::none
                    || storage_flag == Flags::Storage::zero
                    || storage_flag == Flags::Storage::constant) {
                    
                    continue;
                }
                
                switch(_columns[x].flags.type) {
                    default:
                        LOGERR("invalid type value (%02x)\n", (int)_columns[x].flags.type);
                        break;
                    case Flags::Type::i8:
                    case Flags::Type::i8t:
                        file.write(_rows[y][x].data.i8);
                        break;
                    case Flags::Type::i16:
                    case Flags::Type::i16t:
                        file.write(_rows[y][x].data.i16);
                        break;
                    case Flags::Type::f32:
                        file.write(_rows[y][x].data.flt);
                        break;
                    case Flags::Type::i32:
                    case Flags::Type::i32t:
                        file.write(_rows[y][x].data.i32);
                        break;
                    case Flags::Type::i64:
                    case Flags::Type::i64t:
                        file.write(_rows[y][x].data.i64);
                        break;
                    case Flags::Type::str:
                        if(!_rows[y][x].data.string) {
                            file.write<uint32_t>(0);
                            break;
                        }
                        
                        file.write<uint32_t>(save_new_string(_rows[y][x].data.string));
                        break;
                    case Flags::Type::data:
                        LOGERR("data not implemented");
                        break;
                }
            }
        }
        
        uint32_t strings_offset = file.tell() - start_offset;
        LOGINF("strings offset: %d", strings_offset);
        for(int i = 0; i < strings.size(); i++) {
            file.write((uint8_t*)strings[i].data(), strings[i].length());
            file.write('\0');
        }
        //file.write_file(strings); //save string table
        uint32_t data_offset = file.tell() - start_offset;
        //data here? we don't handle that yet though
        uint32_t table_size = file.tell() - start_offset;
        size_t finish_offset = file.tell();
        
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
        
        size_t offset = file.tell();
        uint32_t magic = file.read<uint32_t>();

        file.endian(Tea::Endian::big);
        file.read(_table_size);
        _rows_offset = file.read<uint32_t>() + offset + 8;
        _strings_offset = file.read<uint32_t>() + offset + 8;
        _data_offset = file.read<uint32_t>() + offset + 8;

        uint32_t table_name_offset;
        file.read(table_name_offset);
        file.read(_num_columns);
        file.read(_row_length);
        file.read(_num_rows);

        _columns.resize(_num_columns);
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
            }
        }

        file.seek(table_name_offset + _strings_offset);
        while(char curr = file.read<char>()) {
            _table_name += curr;
        }
        
        _rows.resize(_num_rows);
        for(int i = 0; i < _rows.size(); i++) {
            _rows[i].resize(_num_columns);
            
            file.seek(_rows_offset + (i * _row_length));
            
            for(int a = 0; a < _num_columns; a++) {

                Flags::Storage storage_flag = _columns[a].flags.storage;
                if(storage_flag == Flags::Storage::none
                    || storage_flag == Flags::Storage::zero
                    || storage_flag == Flags::Storage::constant) {
                    
                    continue;
                }
                
                //HACK: what about Storage::constant?
                
                switch(_columns[a].flags.type) {
                    default:
                        LOGERR("invalid type value (%02x)\n", (int)_columns[a].flags.type);
                        break;
                    case Flags::Type::i8:
                    case Flags::Type::i8t:
                        file.read(_rows[i][a].data.i8);
                        break;
                    case Flags::Type::i16:
                    case Flags::Type::i16t:
                        file.read(_rows[i][a].data.i16);
                        break;
                    case Flags::Type::i32:
                    case Flags::Type::i32t:
                        file.read(_rows[i][a].data.i32);
                        break;
                    case Flags::Type::i64:
                    case Flags::Type::i64t:
                        file.read(_rows[i][a].data.i64);
                        break;
                    case Flags::Type::f32:
                        file.read(_rows[i][a].data.flt);
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
                        _rows[i][a].data.string = (char*)malloc(str_len+1);
                        memcpy(_rows[i][a].data.string, str.c_str(), str_len);
                        _rows[i][a].data.string[str_len] = '\0';
                        file.seek(tmp_offset + 4);
                        break;
                    }
                    case Flags::Type::data:
                        LOGERR("data not implemented");
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
    
    bool open_directory(std::string& directory);
    bool save_directory(std::string& directory);

    struct Entry {
        //data:
        Tea::File* file = nullptr;
        
        //TOC:
        std::string dirname;
        std::string filename;
        uint64_t filesize;
        uint64_t extractsize;
        std::string userstring;
        uint32_t ID;
        
        //ETOC:
        std::string localdir;
        uint64_t updatedatetime;
    };
    std::vector<Entry> _filetable;
    
    std::vector<uint8_t> _gtoc_data;
    
    
    UTF_Table _cpk_table;
    UTF_Table _toc_table;
    UTF_Table _etoc_table;
    
private:
    Tea::File* _file = nullptr;
};

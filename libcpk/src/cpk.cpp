#include "cpk.hpp"

#include <string.h>
#include <string>
#include <typeinfo>
#include <filesystem>
#include <algorithm>
#include <functional>
namespace fs = std::filesystem;

#include <logging.hpp>

// https://github.com/ConnorKrammer/cpk-tools/blob/master/LibCPK/CPK.cs


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
    uint64_t utf_size; file.read(utf_size);
    out_data.resize(utf_size);
    file.read(out_data.data(), utf_size);
    if(out_data[0] != 0x40 || out_data[1] != 0x55 || out_data[2] != 0x54 || out_data[3] != 0x46) { //@UTF
        decryptUTF(out_data);
    }
}

bool CPK::save(Tea::File& file) {
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
    LOGINF("total data size (excluding padding): %d", current_data_size);
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

bool CPK::open_directory(std::string& directory) {
    
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
    
    for(int i = 0; i < files.size(); i++) {
        fs::path& p = files[i];
        fs::path rel_dir_path = fs::relative(p, fs_dir).parent_path();
        
        _toc_table._rows.resize(i+1);
        _etoc_table._rows.resize(i+1);
        _toc_table._rows[i].resize(_toc_table._columns.size());
        _etoc_table._rows[i].resize(_etoc_table._columns.size());
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
        
        LOGINF("time: y=%d, m=%d, d=%d", gmt->tm_year + 1900, gmt->tm_mon, gmt->tm_mday);
        
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
    
    _etoc_table._rows.resize(_etoc_table._rows.size()+1);
    _etoc_table._rows[_etoc_table._rows.size() - 1].resize(_etoc_table._columns.size());
    _etoc_table.set<uint64_t>(0, col_etoc_datetime, _etoc_table._rows.size() - 1);
    _etoc_table.set<char*>("", col_etoc_dirname, _etoc_table._rows.size() - 1);

    
    return true;
}

bool CPK::open_empty(Tea::File& file) {
    this->close();
    _file = &file;
    return this->open_empty();
}

bool CPK::open_empty() {
    //TODO: initialize utf tables with default columns
    
    _cpk_table._table_name = "CpkHeader";
    _toc_table._table_name = "CpkTocInfo";
    _etoc_table._table_name = "CpkEtocInfo";
    
    //default initialization values
#define TF(type, storage, string) {(UTF_Table::Flags){UTF_Table::Flags::Type::type, UTF_Table::Flags::Storage::storage}, string}
#define CE(...) _cpk_table._columns.push_back(TF(__VA_ARGS__))
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
    _cpk_table._rows.resize(1);
    _cpk_table._rows[0].resize(_cpk_table._columns.size());
#undef CE
#define TE(...) _toc_table._columns.push_back(TF(__VA_ARGS__))
    _toc_table._columns.resize(0);
    TE(str, per_row, "DirName");
    TE(str, per_row, "FileName");
    TE(i32, per_row, "FileSize");
    TE(i32, per_row, "ExtractSize");
    TE(i64, per_row, "FileOffset");
    TE(i32, per_row, "ID");
    TE(str, constant, "UserString");
#undef TE
#define EE(...) _etoc_table._columns.push_back(TF(__VA_ARGS__))
    _etoc_table._columns.resize(0);
    EE(i64, per_row, "UpdateDateTime");
    EE(str, per_row, "LocalDir");
#undef TF
    
    
    _cpk_table.set_by_name<uint64_t>(1, "UpdateDateTime", 0);
    
    return true;
}

//TODO: those fancy file abstractions are cool and all, but for the love of god just integrate readUTF and decryptUTF with the UTF_table class. reading this code is painful
bool CPK::open(Tea::File& file) {
    this->close();

    _file = &file;

    char magic[5] = "\0\0\0\0";
    if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading CPK magic"); return false; }
    if(strncmp(magic, "CPK ", 4)) { LOGERR("expected CPK magic, but didn't get it"); return false; }

    std::vector<uint8_t> CPK_packet;
    readUTF(*_file, CPK_packet);
    Tea::FileMemory cpk_file_packet(*CPK_packet.data(), CPK_packet.size());
    _cpk_table.open(cpk_file_packet);
    
    { //read TOC and open files
        int toc_offset_column = _cpk_table.get_column("TocOffset");
        int toc_size_column = _cpk_table.get_column("TocSize");
        if(toc_offset_column == -1 || toc_size_column == -1) { LOGERR("TocOffset or TocSize not found in CPK header"); return false; }
        uint64_t toc_offset;
        _cpk_table.get(toc_offset, toc_offset_column, 0);
        _file->seek(toc_offset);
        
        if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading TOC magic"); return false; }
        if(strncmp(magic, "TOC ", 4)) { LOGERR("expected TOC magic, but didn't get it"); return false; }
        std::vector<uint8_t> toc_packet;
        readUTF(*_file, toc_packet);
        Tea::FileMemory toc_file_packet(*toc_packet.data(), toc_packet.size());
        _toc_table.open(toc_file_packet);
        
        _filetable.resize(_toc_table._num_rows);
        for(int i = 0; i < _toc_table._num_rows; i++) {
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
            
            Tea::FileMemory* mem = new Tea::FileMemory();
            mem->open_owned();
            _file->seek(file_offset + 2048);
            mem->write_file(file, _filetable[i].filesize);
            
            _filetable[i].file = mem;
            
            //TODO: decompress files if compressed (or maybe do this when saving to disk or something?
            LOGVER("size %010d, extractsize %010d, dir %s, name %s", _filetable[i].filesize, _filetable[i].extractsize, _filetable[i].dirname.c_str(), _filetable[i].filename.c_str());

        }
    }
    
    { //read ETOC
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
        
        if(_filetable.size() != _etoc_table._rows.size()) {
            LOGWAR("ETOC table doesn't contain the same amount of entries as the TOC table, which might be quite bad (yes, I don't know if it is) (%d vs %d)", _filetable.size(), _etoc_table._rows.size());
        }
        
        for(int i = 0; i < std::min(_filetable.size(), _etoc_table._rows.size()); i++) {
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
            LOGINF("no ITOC in this file");
            goto after_itoc;
        }
        
        if(!_file->read((uint8_t*)magic, 4)) { LOGERR("error reading ITOC magic"); return false; }
        if(strncmp(magic, "ITOC", 4)) { LOGERR("expected ITOC magic, but didn't get it"); return false; }
        std::vector<uint8_t> itoc_packet;
        readUTF(*_file, itoc_packet);
        UTF_Table itoc_table;
        Tea::FileMemory itoc_file_packet(*itoc_packet.data(), itoc_packet.size());
        itoc_table.open(itoc_file_packet);
        
        LOGERR("NOT IMPLEMENTED: ITOC reading");
    }
after_itoc:
    
    { //read GTOC
        //HACK we ignore GTOC for now, fix this maybe
        LOGWAR("ignoring possible GTOC section");
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
    
    return true;
}

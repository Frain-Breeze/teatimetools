#include "cpk.hpp"

#include <string.h>
#include <string>
#include <typeinfo>

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
    
    //query file sizes
    size_t current_offset = 2048;
    size_t offset_row = _toc_table.get_column("FileOffset");
    size_t size_row = _toc_table.get_column("FileSize");
    size_t extractsize_row = _toc_table.get_column("ExtractSize");
    //TODO: assert filetable size == toc table size (and do something if it's not, like resizing toc table)
    for(int i = 0; i < _filetable.size(); i++) {
        while(current_offset % 2048) { current_offset++; }
        _toc_table._rows[i][offset_row].data.i64 = current_offset;
        _toc_table._rows[i][size_row].data.i64 = _filetable[i].file->size();
        _toc_table._rows[i][extractsize_row].data.i64 = _filetable[i].extractsize;//_filetable[i].file->size(); //TODO: fix this
        current_offset += _filetable[i].file->size();
        
        LOGVER("updates file with size %10d, offset %10d (%016x) and name %s", _filetable[i].file->size(), _toc_table._rows[i][offset_row].data.i64, _toc_table._rows[i][offset_row].data.i64, _filetable[i].filename.c_str());
    }
    
    size_t end_of_data = current_offset;
    
    for(int i = 0; i < _filetable.size(); i++) {
        file.seek(_toc_table._rows[i][offset_row].data.i64);
        _filetable[i].file->seek(0); //reset data file position
        file.write_file(*_filetable[i].file, _filetable[i].file->size());
    }
    
    //disable GTOC (since we don't include it)
    _cpk_table.set_by_name<uint64_t>(0, "GtocOffset", 0);
    _cpk_table.set_by_name<uint64_t>(0, "GtocSize", 0);
    
    //save TOC
    file.seek(end_of_data);
    while(file.tell() % 2048) { file.skip(1); }
    {
        _cpk_table.set_by_name<uint64_t>(file.tell(), "TocOffset", 0);
        size_t before_toc_packet = file.tell();
        
        
        file.write((uint8_t*)"TOC \xff\0\0\0", 8);
        file.skip(8); //TOC packet size
        
        size_t before_toc = file.tell();
        _toc_table.save(file);
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
        _cpk_table.set_by_name<uint64_t>(file.tell(), "EtocOffset", 0);
        size_t before_etoc_packet = file.tell();
        
        
        file.write((uint8_t*)"ETOC\xff\0\0\0", 8);
        file.skip(8); //TOC packet size
        
        size_t before_etoc = file.tell();
        _etoc_table.save(file);
        uint64_t etoc_size = file.tell() - before_etoc;
        
        _cpk_table.set_by_name<uint64_t>(file.tell() - before_etoc_packet, "EtocSize", 0);
        
        file.seek(-etoc_size - 8, Tea::Seek_current);
        file.write<uint64_t>(etoc_size);
    }
    
    //paste CPK header into file
    file.seek(16);
    _cpk_table.save(file);
    uint64_t cpk_table_size = file.tell() - 16 + 4;
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

bool CPK::open_empty(Tea::File& file) {
    //TODO: initialize utf tables with default columns
    this->close();
    _file = &file;
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
            _file->seek(file_offset);
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

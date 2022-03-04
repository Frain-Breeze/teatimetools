/*
 constraints:
 - must be able to go to next and previous entry
 - must be able to stream file from disk (duh)
 - possible checkpoint system? (see below)
 
 checkpoints:
 for compressed archive entries, it might be good to have a way to create a checkpoint of sorts, so that you can always go back to that part in the file without having to re-read from the start

*/

#pragma once

#include <string>

#include "teaio_file.hpp"

//TODO: continue
/*namespace Tea {
	
	struct ArchiveEntry {
		std::string path;
		std::string name;
		Tea::File* data = nullptr;
	};
	
	class Archive {
	public:
		virtual bool close() = 0;
		
		virtual int num_entries() = 0;
		virtual ArchiveEntry* get_entry(int entry) = 0;
		
		virtual ~Archive();
	protected:
		Tea::File* _file;
	};
	
	class ArchiveDigitalcute : public Archive {
		~ArchiveDigitalcute() {this->close(); }
		bool close() {}
		
		bool open(ea::File& file) { _file = file; }
		
	};
}*/
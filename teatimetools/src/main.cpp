#include <map>
#include <string>
#include <filesystem>
#include <set>
#include <vector>
namespace fs = std::filesystem;

#include <fmdx.hpp>
#include <uvr.hpp>
#include <tts.hpp>
#include "logging.hpp"
#include <string.h>

#ifdef TEA_ENABLE_ISO
#include <archive.h>
#include <archive_entry.h>
#endif

#ifdef TEA_ENABLE_CPK
#include <cpk.hpp>
#endif

#ifdef TEA_ON_WINDOWS
#include <windows.h>
#endif

struct settings {
	std::string inpath;
	std::string outpath;
    std::string lastpath;
};

namespace proc {
	bool fmdx_unpack(settings& set) {
		size_t offs = set.inpath.find("PSP_GAME", 0);
		std::string in = set.inpath.substr(0, offs);
		std::string mid = set.inpath.substr(offs, std::string::npos);
        size_t out_offs = set.outpath.find("PSP_GAME", 0);
        std::string out = set.outpath.substr(0, out_offs);
		return fmdx_extract(in, mid, out);
	}
	bool fmdx_pack(settings& set) {
        size_t offs = set.inpath.find("PSP_GAME", 0);
		std::string in = set.inpath.substr(0, offs);
		std::string mid = set.inpath.substr(offs, std::string::npos);
        size_t out_offs = set.outpath.find("PSP_GAME", 0);
        std::string out = set.outpath.substr(0, out_offs);
		return fmdx_repack(in, mid, out);
	}
	bool uvr_unpack(settings& set) {
		return uvr_extract(set.inpath, set.outpath);
	}
	bool uvr_pack(settings& set) {
		return uvr_repack(set.inpath, set.outpath);
	}
	bool tts_unpack(settings& set) {
        return tts_extract(set.inpath, set.outpath);
    }
    bool tts_pack(settings& set) {
        return tts_repack(set.inpath, set.outpath);
    }
    bool convo_extract(settings& set) {
        return conversation_extract(set.inpath, set.outpath, set.lastpath);
    }
}

#ifdef TEA_ENABLE_CPK
namespace proc_cpk {
    bool cpk_unpack(settings& set) {
        CPK cpk;
        Tea::FileDisk in;
        in.open(set.inpath.c_str(), Tea::Access_read);
        cpk.open(in);
        
        cpk.save_directory(set.outpath);
        return true;
    }
    bool cpk_pack(settings& set) {
        CPK cpk;
        cpk.open_directory(set.inpath);
        
        Tea::FileDisk out;
        out.open(set.outpath.c_str(), Tea::Access_write);
        cpk.save(out);
        return true;
    }
}
#endif

#ifdef TEA_ENABLE_ISO
namespace proc_iso {
    bool iso_unpack(settings& set) {
        archive* a = archive_read_new();
        archive_read_support_format_iso9660(a);
        int r = archive_read_open_filename(a, set.inpath.c_str(), 16384);
        if(r != ARCHIVE_OK) { LOGERR("couldn't open archive %s, error: %s", set.inpath.c_str(), archive_error_string(a)); return false; }
        
        archive_entry* ent;
        while(archive_read_next_header(a, &ent) == ARCHIVE_OK) {
            LOGVER("%s", archive_entry_pathname(ent));
            
            if(archive_entry_filetype(ent) != AE_IFREG)
                continue;
            
            size_t outsize = archive_entry_size(ent);
            uint8_t* out_data = (uint8_t*)malloc(outsize);
            archive_read_data(a, out_data, outsize);
            
            fs::path out_path = set.outpath;
            out_path /= archive_entry_pathname_utf8(ent);
            
            fs::create_directories(out_path.parent_path());
            
            FILE* fo = fopen(out_path.u8string().c_str(), "wb");
            if(!fo) {
                LOGERR("couldn't open output file %s for writing", out_path.u8string().c_str());
                return false;
            }
            fwrite(out_data, outsize, 1, fo);
            fclose(fo);
            
            free(out_data);
        }
        return true;
    }
    bool iso_pack(settings& set) {
        archive* a = archive_write_new();
        archive_write_set_format_iso9660(a);
        if(archive_write_set_options(a, "iso-level=4,volume-id=,application-id=PSP GAME,publisher=") != ARCHIVE_OK) {
            LOGERR("couldn't set iso options. error: %s", archive_error_string(a));
            return false;
        }
        if(archive_write_open_filename(a, set.outpath.c_str()) != ARCHIVE_OK) {
            LOGERR("couldn't open %s for writing", set.outpath.c_str());
            return false;
        }
        
        for(const auto& p : fs::recursive_directory_iterator(set.inpath)) {
            if(fs::is_regular_file(p)) {
                fs::path rel_path = fs::relative(p, set.inpath);
                
                archive_entry* ent = archive_entry_new();
                archive_entry_set_pathname(ent, rel_path.u8string().c_str());
                archive_entry_set_size(ent, fs::file_size(p.path()));
                archive_entry_set_filetype(ent, AE_IFREG);
                archive_entry_set_perm(ent, 0644);
                archive_write_header(a, ent);
                
                FILE* fi = fopen(p.path().u8string().c_str(), "rb");
                fseek(fi, 0, SEEK_END);
                size_t file_size = ftell(fi);
                fseek(fi, 0, SEEK_SET);
                uint8_t* buff = (uint8_t*)malloc(file_size);
                fread(buff, file_size, 1, fi);
                fclose(fi);
                
                archive_write_data(a, buff, file_size);
                
                LOGVER("added entry %s (%s) with size %d", rel_path.u8string().c_str(), p.path().u8string().c_str(), fs::file_size(p.path()));
                
                archive_entry_free(ent); //TODO: reuse
                free(buff); //TODO: use smaller buffer instead of malloc/free-ing constantly
            }
        }
        
        archive_write_close(a);
        archive_write_free(a);
    }
}
#endif

namespace proc_helper {
    bool copy(settings& set) {
        fs::copy(set.inpath, set.outpath);
        return true;
    }
    
    bool move(settings& set) {
        fs::rename(set.inpath, set.outpath);
        return true;
    }
    
    bool print(settings& set) {
        LOGALWAYS("mod list: %s", set.inpath.c_str());
        return true;
    }
}

typedef bool (*procfn)(settings& set);

struct comInfo {
    const char* const help_string;
    enum Required_type {
        Rno,
        Rfile,
        Rdir,
        Reither,
    };
    Required_type inpath_required;
    Required_type outpath_required;
    Required_type lastpath_required;

    procfn fn;
};

bool list_executer(settings& set);

static std::map<std::string, comInfo> infoMap{
    {"execute_list", {"process every line in the input file as standalone command (useful for mods)", comInfo::Rfile, comInfo::Rno, comInfo::Rno, list_executer} },
	{"fmdx_unpack", {"unpacks fmdx archives (.bin)", comInfo::Rfile, comInfo::Rdir, comInfo::Rno, proc::fmdx_unpack} },
	{"fmdx_pack", {"repacks fmdx archives (.bin)", comInfo::Rdir, comInfo::Rfile, comInfo::Rno, proc::fmdx_pack} },
	{"uvr_unpack", {"converts .uvr images to .png", comInfo::Rfile, comInfo::Rfile, comInfo::Rno, proc::uvr_unpack} },
    {"uvr_pack", {"converts images to .uvr", comInfo::Rfile, comInfo::Rfile, comInfo::Rno, proc::uvr_pack} },
    {"tts_unpack", {"unpacks event files (only the ones in teatime_event/Event/ currently)", comInfo::Rfile, comInfo::Rdir, comInfo::Rno, proc::tts_unpack} },
    {"tts_pack", {"repacks event files, only teatime_event/Event/ format", comInfo::Rdir, comInfo::Rfile, comInfo::Rno, proc::tts_pack} },
    {"convo_extract", {"in: conversation data (.bin), middle: fontsheet (.png), out: output image (.png)", comInfo::Rfile, comInfo::Rfile, comInfo::Rfile, proc::convo_extract} },
    
    //optionally built options
#ifdef TEA_ENABLE_CPK
    {"cpk_unpack", {"put all files from the .cpk file into a folder", comInfo::Rdir, comInfo::Rfile, comInfo::Rno, proc_cpk::cpk_unpack} },
    {"cpk_pack", {"put all files from a folder into a .cpk file", comInfo::Rfile, comInfo::Rdir, comInfo::Rno, proc_cpk::cpk_pack} },
#endif
#ifdef TEA_ENABLE_ISO
    {"iso_unpack", {"put all files from the .iso file into a folder", comInfo::Rdir, comInfo::Rfile, comInfo::Rno, proc_iso::iso_unpack} },
    {"iso_pack", {"put all files from a folder into a .iso file", comInfo::Rfile, comInfo::Rdir, comInfo::Rno, proc_iso::iso_pack} },
#endif
    
    //helper functions (for mod list things)
    {"helper_copy", {"copy file/folder from input to output path", comInfo::Reither, comInfo::Reither, comInfo::Rno, proc_helper::copy} },
    {"helper_move", {"move file/folder from input to output path, can also be used for renaming", comInfo::Reither, comInfo::Reither, comInfo::Rno, proc_helper::move} },
    {"helper_print", {"print input argument in console", comInfo::Reither, comInfo::Rno, comInfo::Rno, proc_helper::print} },
};

void func_handler(settings& set, procfn fn){
    LOGBLK
    //TODO: do better path checking, creation, etc

    if(!set.outpath.empty()) {
        fs::path outpath = fs::u8path(set.outpath).parent_path();
        if(!outpath.empty()) {
            fs::create_directories(outpath);
        }
    }

    fn(set);
}

void help_print() {
    LOGALWAYS("most basic interface: 'teatimetools <input file/folder>'. the program will figure out which of the commands to use.");
    LOGALWAYS("");
    LOGALWAYS("advanced interface: 'teatimetools <command> -i=<input> -o=<output> -m=<middle_path> (optional: -l<logging> -d=<.extension:.extension> -r)'.");
    LOGALWAYS("    different commands use different parameters (see list at the bottom). order is irrelevant.");
    LOGALWAYS("");
    LOGALWAYS("option explanation:");
    {
        LOGBLK
        LOGALWAYS("-l, logging: there are four logging channels available. Error (e), Warning (w), Info (i), and Verbose (v). use + to turn on a channel, and - to turn one off.");
        LOGALWAYS("    by default, all channels except verbose are enabled.");
        LOGALWAYS("    for example: -l+v turns on verbose.");
        LOGALWAYS("                 -l-ewi turns off error, warning, and info.");
        LOGALWAYS("                 -l+v-ewi turns off error, warning, and info, but turns verbose on.");
        LOGALWAYS("-d, directory scan: do the operation for every file in the input directory, if it matches the supplied extensions.");
        LOGALWAYS("    for example: -d=.uvr:.png runs the command for every .uvr file, and saves them as .png files.");
        LOGALWAYS("                 -d=_ ignores the extension, and simply does the command for every file");
        LOGALWAYS("                 -d=.uvr runs for every .uvr file, but will also save the output as .uvr (as no output extension is provided)");
        LOGALWAYS("-r, recursive: makes the -d option recursive. that's all.");
    }
    
    LOGALWAYS("");
    LOGALWAYS("here are all the currently implemented commands:");
    LOGALWAYS("D = directory, F = file, E = either file/directory, - = not required");
    LOGALWAYS("name / input required - middle required - output required / explanation");
    LOGBLK;
    for(const auto& a : infoMap) {
        char in_req = '-';
        char mid_req = '-';
        char out_req = '-';
        if(a.second.inpath_required == comInfo::Rdir) { in_req = 'D'; }
        if(a.second.inpath_required == comInfo::Rfile) { in_req = 'F'; }
        if(a.second.inpath_required == comInfo::Reither) { in_req = 'E'; }
        if(a.second.outpath_required == comInfo::Rdir) { out_req = 'D'; }
        if(a.second.outpath_required == comInfo::Rfile) { out_req = 'F'; }
        if(a.second.outpath_required == comInfo::Reither) { out_req = 'E'; }
        if(a.second.lastpath_required == comInfo::Rdir) { mid_req = 'D'; }
        if(a.second.lastpath_required == comInfo::Rfile) { mid_req = 'F'; }
        if(a.second.lastpath_required == comInfo::Reither) { mid_req = 'E'; }
        LOGALWAYS("%-20s %c%c%c %s", a.first.c_str(), in_req, mid_req, out_req, a.second.help_string);
    }
}

int main_executer(int argc, char* argv[]) {
    logging::set_channel(logging::Cerror, true);
    logging::set_channel(logging::Cwarning, true);
    logging::set_channel(logging::Cinfo, true);
    logging::set_channel(logging::Cverbose, false);

    if(argc < 2){
        //print help
        help_print();
    }else{
        comInfo* cinf = nullptr;
        settings set;

        std::string search_extension = "";
        std::string save_extension = "";
        bool recursive = false;


        for(int i = 1; i < argc; i++){
            if(argv[i][0] == '-' && argv[i][1] != '\0'){
                std::string carg = argv[i];

                if(carg[2] == '=') { //for all commands that require an argument
                    if(carg[1] == 'i') { set.inpath = carg.substr(3, std::string::npos); }
                    if(carg[1] == 'm') { set.lastpath = carg.substr(3, std::string::npos); }
                    if(carg[1] == 'o') { set.outpath = carg.substr(3, std::string::npos); }
                    if(carg[1] == 'd') {
                        size_t separator = carg.find_first_of(':');
                        search_extension = carg.substr(3, separator - 3);
                        LOGINF("using %s as file mask", search_extension.c_str());
                        if(separator != std::string::npos) {
                            save_extension = carg.substr(separator+1, std::string::npos);
                            LOGINF("using %s as file save extension", save_extension.c_str());
                        }
                        else { save_extension = search_extension; }
                    }
                }

                if(carg[1] == 'r'){ recursive = true; }
                if(carg[1] == 'l'){
                    //Logging settings. use - to set mode to "turn off", use + to set mode to "turn on".
                    //example: -l+vewi turns on all channels
                    // -l-e+v turns error off (-e), and verbose on (+v)
                    //default mode is "turn on".
                    bool mode = true;
                    size_t progress = 2;
                    while(argv[i][progress] != '\0'){
                        switch(argv[i][progress]){
                            case '-': mode = false; break;
                            case '+': mode = true; break;
                            case 'v': logging::set_channel(logging::Cverbose, mode); break;
                            case 'e': logging::set_channel(logging::Cerror, mode); break;
                            case 'w': logging::set_channel(logging::Cwarning, mode); break;
                            case 'i': logging::set_channel(logging::Cinfo, mode); break;
                            default: {
                                LOGERR("argument \"%s\" could not be parsed properly. char '%c' is unknown", argv[i], argv[i][progress]);
                                argv[i][progress+1] = '\0'; //break out of while loop
                                break;
                            }
                        }
                        progress++;
                    }
                }
            }else{
                auto found = infoMap.find(argv[i]);
                if (found != infoMap.end()) {
                    cinf = &found->second;
                }
            }
        }

        if(!cinf->fn){
            LOGERR("didn't find operation to do in the arguments supplied!\n");
            return 0;
        }

        //do verbose logging of passed parameters
        LOGVER("input path:  %s", (set.inpath.length()) ? set.inpath.c_str() : "<not given>");
        LOGVER("output path: %s", (set.outpath.length()) ? set.outpath.c_str() : "<not given>");
        LOGVER("last path:   %s", (set.lastpath.length()) ? set.lastpath.c_str() : "<not given>");
        if(recursive) { LOGVER("using recursive search"); }
        if(search_extension.length()) {
            if(search_extension == "_") { LOGVER("processing all files in the input folder"); }
            else { LOGVER("processing all files in the input folder with .%s as extension", search_extension.c_str()); }
        }

        //check for required parameters
        auto check_parameters = [](comInfo::Required_type req_type, std::string path, const char* name, bool should_exist) -> bool {
            if(req_type == comInfo::Rno) { return true; }
            else if(req_type == comInfo::Rdir) {
                if(should_exist && !fs::exists(path)) { LOGNERR("%s path does not exist", "check_parameters", name); return false; }
                if(!fs::is_directory(path)) { LOGNERR("%s path is not a directory", "check_parameters", name); return false; }
                LOGNVER("%s path is a directory, as required", "check_parameters", name);
                return true;
            }
            else if(req_type == comInfo::Rfile) {
                if(should_exist && !fs::exists(path)) { LOGNERR("%s file does not exist", "check_parameters", name); return false; }
                if(should_exist && !fs::is_regular_file(path)) { LOGNERR("%s path is not a file", "check_parameters", name); return false; }
                LOGNVER("%s path is a file, as required", "check_parameters", name);
                return true;
            }
            return false;
        };

        if(search_extension == ""){
            func_handler(set, cinf->fn);
        }else {
            bool care_about_extension = (search_extension == "_") ? false : true;
            std::string real_in = set.inpath;
            std::string real_out = set.outpath;
            std::string real_mid = set.lastpath;

            auto func = [&](const fs::path& p){
                set.inpath = p.u8string();

                fs::path rel_path = fs::relative(p, fs::u8path(real_in));

                if((care_about_extension && p.extension() == search_extension) || !care_about_extension){
                    std::string rel_part = rel_path.u8string();
                    fs::path outpath = real_out;
                    outpath /= rel_path.parent_path();
                    outpath /= rel_path.stem();
                    //LOGERR("rel: %s", rel_path.u8string().c_str());
                    //LOGERR("out: %s", real_out.c_str());
                    outpath += save_extension;
                    set.outpath = outpath.u8string();
                    //LOGERR("full out: %s", set.outpath.c_str());

                    outpath = real_mid;
                    outpath /= rel_part;
                    set.lastpath = outpath.u8string();

                    bool should_die = false;
                    should_die |= !check_parameters(cinf->inpath_required, set.inpath, "input", true);
                    should_die |= !check_parameters(cinf->lastpath_required, set.lastpath, "middle", true);
                    should_die |= !check_parameters(cinf->outpath_required, set.outpath, "output", false);

                    //TODO: bad, fix this (with recursive and -d=.bin:.bin)
                    //if(should_die) { LOGERR("aborting due to above errors\n"); return -1; }

                    LOGINF("handling file %s:", rel_part.c_str());

                    func_handler(set, cinf->fn);
                }
            };

            if(recursive){
                for(const auto& p : fs::recursive_directory_iterator(real_in)){
                    func(p.path());
                }
            }else{
                for(const auto& p : fs::directory_iterator(real_in)){
                    func(p.path());
                }
            }
        }
    }

    printf("\n");
    return 0;
}

//TODO: enforce all directories to be relative to the mod.txt
bool list_executer(settings& set) {
    FILE* fi = fopen(set.inpath.c_str(), "r");
    if(!fi) { LOGERR("couldn't open file %s for reading!", set.inpath.c_str()); return false; }

    char line_buffer[4096];

    //HACK: set current directory equal to where mod.txt is located, figure something out to make this less issue-prone (proper docs, etc)
    fs::current_path(fs::u8path(set.inpath).parent_path());

    while(fgets(line_buffer, 4096, fi) == line_buffer) {
        std::vector<std::string> parsed_argv;
        parsed_argv.push_back("hello.exe");

        std::string curr = "";
        bool inside_string = false;
        for(int i = 0; i < 4096; i++) {
            if(line_buffer[i] == '\"') { inside_string = !inside_string; }
            else if(line_buffer[i] == '\\') { continue; }
            else if(line_buffer[i] == '\n' || line_buffer[i] == ' ') {
                if(!curr.empty()) { parsed_argv.push_back(curr); }
                curr.clear();
                continue;
            }
            else if(line_buffer[i] == '\0') {
                if(!curr.empty()) { parsed_argv.push_back(curr); }
                break;
            }
            curr += line_buffer[i];
        }

        std::vector<char*> argv_ptrs(parsed_argv.size());
        for(int i = 0; i < argv_ptrs.size(); i++) {
            argv_ptrs[i] = parsed_argv[i].data();
        }
        main_executer(parsed_argv.size(), argv_ptrs.data());
    }

    return true;
}

bool drag_drop_solver(char* char_argument_one, char* char_argument_two) {
    const std::string argument = char_argument_one;
    const std::string argument_two = (char_argument_two) ? char_argument_two : argument;
    const fs::path path_argument = fs::u8path(argument);
    const fs::path path_out_argument = fs::u8path(argument_two);
    fs::path path_out_no_ext = path_out_argument.parent_path(); path_out_no_ext /= path_out_argument.stem();
    
    settings set;
    std::string function = "";
    
    if(fs::exists(argument)) {
        std::string extension = path_argument.extension().u8string();
        if(fs::is_directory(argument)) {
            set.inpath = argument;
            set.outpath = path_out_no_ext.u8string();
            if (extension == ".png") { //TODO: add all other image types (do this for the uvr processing in general)
                function = "uvr_pack";
                set.outpath += ".uvr";
            }else if(extension == ".tts_folder") {
                function = "tts_pack";
                set.outpath += ".tts";
            }else if(extension == ".fmdx_folder") {
                function = "fmdx_pack";
                set.outpath += ".bin";
            }else if(extension == ".iso_folder") {
                function = "iso_pack";
                set.outpath += ".iso";
            }else if(extension == ".cpk_folder") {
                function = "cpk_pack";
                set.outpath += ".cpk";
            }else {
                LOGERR("argument is a directory, but desired operation could not be deduced.");
                return false;
            }
        }
        else if(fs::is_regular_file(argument)) {
            set.inpath = argument;
            set.outpath = path_out_no_ext.u8string();
            if(extension == ".uvr") {
                function = "uvr_unpack";
                set.outpath += ".png";
            }else if(extension == ".bin") {
                function = "fmdx_unpack";
                set.outpath += ".fmdx_folder";
            }else if(extension == ".tts") {
                function = "tts_unpack";
                set.outpath += ".tts_folder";
            }else if(extension == ".txt") {
                function = "execute_list";
                set.outpath = "";
            }else if(extension == ".cpk") {
                function = "cpk_unpack";
                set.outpath += ".cpk_folder";
            }else if(extension == ".iso") {
                function = "iso_unpack";
                set.outpath += ".iso_folder";
            }else {
                LOGERR("argument is a file, but desired operation could not be deduced.");
                return false;
            }
        }else {
            LOGERR("argument is not an existing file or directory, so the desired operation can't be deduced");
            return false;
        }
        
        LOGINF("deduced operation: %s. output path is %s", function.c_str(), set.outpath.c_str());
        
        //we have now figured out what to do with it
        const auto found_function = infoMap.find(function);
        if(found_function != infoMap.end()) {
            func_handler(set, found_function->second.fn);
        }else {
            LOGERR("could not find function \"%s\" in infoMap. please report this.", function.c_str());
            return false;
        }
        
    }else {
        LOGERR("argument doesn't exist on disk, so we can't do anything with it");
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    logging::set_channel(logging::Cerror, true);
    logging::set_channel(logging::Cwarning, true);
    logging::set_channel(logging::Cinfo, true);
    logging::set_channel(logging::Cverbose, false);
    
    int ret = 0;
    
    bool try_drag_drop = true;
    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-') { //check if argument is "advanced" (like -i, -o, etc)
            try_drag_drop = false;
            break;
        }
    }
    
    if(try_drag_drop) { //check if we should try for the drag-n-drop interface
        LOGINF("using drag-n-drop-like interface");
        logging::set_channel(logging::Cverbose, true);
        if(argc == 2)
            drag_drop_solver(argv[1], nullptr);
        else if(argc != 1)
            drag_drop_solver(argv[1], argv[2]);
        else
            help_print();
    }else {
        LOGINF("using normal interface");
        ret = main_executer(argc, argv);
    }
    
    
#ifdef TEA_ON_WINDOWS
    //check if we own the console (launched from file explorer), and if so, wait on a keypress to exit
    HWND console_window = GetConsoleWindow();
    DWORD dwProcessID;
    GetWindowThreadProcessId(console_window, &dwProcessID);
    if(GetCurrentProcessId() == dwProcessID) {
        LOGALWAYS("finished. press a key to exit\n");
        getchar();
    }
#endif
    LOGALWAYS(""); //print a last newline
    return ret;
}

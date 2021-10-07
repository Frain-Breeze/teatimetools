#include <map>
#include <string>
#include <filesystem>
#include <set>
namespace fs = std::filesystem;

#include <fmdx.hpp>
#include <uvr.hpp>
#include <tts.hpp>
#include "logging.hpp"
#include <string.h>

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

typedef bool (*procfn)(settings& set);
static std::map<std::string, procfn> procMap{
	{"fmdx_unpack", proc::fmdx_unpack},
	{"fmdx_pack", proc::fmdx_pack},
	{"uvr_unpack", proc::uvr_unpack},
    {"tts_unpack", proc::tts_unpack},
    {"tts_pack", proc::tts_pack},
    {"convo_extract", proc::convo_extract},
};

void pair_test(){
    std::set<uint16_t> pairs;
    const char* test = "Hey everyone, I found this old cassette tape...\nOoo, let's listen to it, Ricchan!\nOk, Urya!\nI'm waiting for you to get your ass back here!!\nEek!?\nUwah! Mio, a ghost is behind you!\nUwaaaaahh!!\nRicchan, you went too far\nAhh, I'm sorry, Mio?\nIt's all right, Mio-chan. There isn't a ghost around.\nReally?\nKyururururin\nMoe moe kyun\0\0\0";

    int total_uncompressed_pairs = 0;
    for(int i = 0; test[i] != 0; i+=2){
        char one = test[i];
        char two = test[i+1];

        if(one == '\n'){i--; continue;}
        if(two == '\n'){two = ' ';}

        total_uncompressed_pairs++;

        const auto found = pairs.find(one | (two << 8));
        if(found == pairs.end()){
            pairs.insert(one | (two << 8));
            printf("inserted %c%c\n", one, two);
        }else{
            printf("duplicate %c%c\n", one, two);
        }
    }
    printf("total pairs compressed: %lu\ntotal pairs if uncompressed: %d\n", pairs.size(), total_uncompressed_pairs);
}

void func_handler(settings& set, procfn fn){
    LOGBLK
    //TODO: do better path checking, creation, etc

    fs::create_directories(fs::u8path(set.outpath).parent_path());

    fn(set);
}

int main(int argc, char* argv[]) {

    logging::set_channel(logging::Cerror, true);
    logging::set_channel(logging::Cwarning, true);
    logging::set_channel(logging::Cinfo, true);
    logging::set_channel(logging::Cverbose, false);

    if(argc < 2){
        LOGERR("not enough arguments supplied!");
    }else{
        procfn fn = nullptr;
        settings set;
        std::string all_in_folder = "";
        bool recursive = false;
        for(int i = 1; i < argc; i++){
            if(argv[i][0] == '-' && argv[i][1] != '\0'){
                if(argc > i){ //all commands that require something after it
                    if(argv[i][1] == 'i'){ set.inpath = argv[i+1]; i++; }
                    else if(argv[i][1] == 'o'){ set.outpath = argv[i+1]; i++; }
                    else if(argv[i][1] == 'm'){ set.lastpath = argv[i+1]; i++; }
                    else if(argv[i][1] == 'd'){ all_in_folder = argv[i+1]; i++; }
                }

                if(argv[i][1] == 'r'){ recursive = true; }
                if(argv[i][1] == 'l'){
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
                auto found = procMap.find(argv[i]);
                if (found != procMap.end()) {
                    fn = found->second;
                }
            }
        }

        if(!fn){
            LOGERR("didn't find operation to do in the arguments supplied!\n");
            return 0;
        }

        if(all_in_folder == ""){
            func_handler(set, fn);
        }else {
            bool care_about_extension = (all_in_folder == "_") ? false : true;
            std::string real_in = set.inpath;
            std::string real_out = set.outpath;
            std::string real_mid = set.lastpath;

            auto func = [&](const fs::path& p){
                set.inpath = p.u8string();

                fs::path rel_path = fs::relative(p, fs::u8path(real_in));

                if((care_about_extension && p.extension() == all_in_folder) || !care_about_extension){
                    std::string rel_part = rel_path.u8string();
                    fs::path outpath = real_out;
                    outpath /= rel_path;
                    set.outpath = outpath.u8string();

                    outpath = real_mid;
                    outpath /= rel_part;
                    set.lastpath = outpath;

                    LOGINF("handling file %s:", rel_part.c_str());

                    func_handler(set, fn);
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
}

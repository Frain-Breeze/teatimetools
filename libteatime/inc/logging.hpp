#pragma once

#include <stddef.h>

//Regular text
#define COLBLK "\e[0;30m"
#define COLRED "\e[0;31m"
#define COLGRN "\e[0;32m"
#define COLYEL "\e[0;33m"
#define COLBLU "\e[0;34m"
#define COLMAG "\e[0;35m"
#define COLCYN "\e[0;36m"
#define COLWHT "\e[0;37m"

//Regular bold text
#define COLBBLK "\e[1;30m"
#define COLBRED "\e[1;31m"
#define COLBGRN "\e[1;32m"
#define COLBYEL "\e[1;33m"
#define COLBBLU "\e[1;34m"
#define COLBMAG "\e[1;35m"
#define COLBCYN "\e[1;36m"
#define COLBWHT "\e[1;37m"

//Regular underline text
#define COLUBLK "\e[4;30m"
#define COLURED "\e[4;31m"
#define COLUGRN "\e[4;32m"
#define COLUYEL "\e[4;33m"
#define COLUBLU "\e[4;34m"
#define COLUMAG "\e[4;35m"
#define COLUCYN "\e[4;36m"
#define COLUWHT "\e[4;37m"

//Regular background
#define COLBLKB "\e[40m"
#define COLREDB "\e[41m"
#define COLGRNB "\e[42m"
#define COLYELB "\e[43m"
#define COLBLUB "\e[44m"
#define COLMAGB "\e[45m"
#define COLCYNB "\e[46m"
#define COLWHTB "\e[47m"

//High intensty background
#define COLBLKHB "\e[0;100m"
#define COLREDHB "\e[0;101m"
#define COLGRNHB "\e[0;102m"
#define COLYELHB "\e[0;103m"
#define COLBLUHB "\e[0;104m"
#define COLMAGHB "\e[0;105m"
#define COLCYNHB "\e[0;106m"
#define COLWHTHB "\e[0;107m"

//High intensty text
#define COLHBLK "\e[0;90m"
#define COLHRED "\e[0;91m"
#define COLHGRN "\e[0;92m"
#define COLHYEL "\e[0;93m"
#define COLHBLU "\e[0;94m"
#define COLHMAG "\e[0;95m"
#define COLHCYN "\e[0;96m"
#define COLHWHT "\e[0;97m"

//Bold high intensity text
#define COLBHBLK "\e[1;90m"
#define COLBHRED "\e[1;91m"
#define COLBHGRN "\e[1;92m"
#define COLBHYEL "\e[1;93m"
#define COLBHBLU "\e[1;94m"
#define COLBHMAG "\e[1;95m"
#define COLBHCYN "\e[1;96m"
#define COLBHWHT "\e[1;97m"

//Reset
#define COLRESET "\e[0m"


#define LOGVER(str, ...) logging::log_advanced(logging::LEVEL::Cverbose, COLBLU"[VERB] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGINF(str, ...) logging::log_advanced(logging::LEVEL::Cinfo, COLRESET"[INFO] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGERR(str, ...) logging::log_advanced(logging::LEVEL::Cerror, COLRED"[ERROR] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGWAR(str, ...) logging::log_advanced(logging::LEVEL::Cwarning, COLYEL"[WARN] %s: " str, __FUNCTION__, ## __VA_ARGS__)

#define LOGBLK logging::block logblock;

namespace logging{

    enum LEVEL : size_t {
        Cwarning,
        Cerror,
        Cinfo,
        Cverbose,
        LEVEL_ITEM_COUNT
    };

    void log_basic(const char* str, ...);
    void log_advanced(LEVEL lvl, const char* str, ...);
    void indent();
    void undent();
    void set_channel(LEVEL lvl, bool state);
    class block{
    public:
        block(){ indent(); }
        ~block(){ undent(); }
    };
}

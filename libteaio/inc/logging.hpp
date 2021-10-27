#pragma once

#include <stddef.h>

//normal text colors
#define COLBLK "\e[30m"
#define COLRED "\e[31m"
#define COLGRN "\e[32m"
#define COLYEL "\e[33m"
#define COLBLU "\e[34m"
#define COLMAG "\e[35m"
#define COLCYN "\e[36m"
#define COLWHT "\e[37m"

#define LOGALWAYS(str, ...) logging::log_basic(COLWHT"" str, ## __VA_ARGS__)
#define LOGVER(str, ...) logging::log_advanced(logging::LEVEL::Cverbose, COLBLU"[VERB] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGINF(str, ...) logging::log_advanced(logging::LEVEL::Cinfo, COLWHT"[INFO] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGERR(str, ...) logging::log_advanced(logging::LEVEL::Cerror, COLRED"[ERROR] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGWAR(str, ...) logging::log_advanced(logging::LEVEL::Cwarning, COLYEL"[WARN] %s: " str, __FUNCTION__, ## __VA_ARGS__)

#define LOGNVER(str, name, ...) logging::log_advanced(logging::LEVEL::Cverbose, COLBLU"[VERB] %s: " str, name, ## __VA_ARGS__)
#define LOGNINF(str, name, ...) logging::log_advanced(logging::LEVEL::Cinfo, COLWHT"[INFO] %s: " str, name, ## __VA_ARGS__)
#define LOGNERR(str, name, ...) logging::log_advanced(logging::LEVEL::Cerror, COLRED"[ERROR] %s: " str, name, ## __VA_ARGS__)
#define LOGNWAR(str, name, ...) logging::log_advanced(logging::LEVEL::Cwarning, COLYEL"[WARN] %s: " str, name, ## __VA_ARGS__)

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

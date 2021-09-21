#pragma once

#define COLRESET "\e[0m"

#define COLBLK "\e[0;30m"
#define COLRED "\e[0;31m"
#define COLGRN "\e[0;32m"
#define COLYEL "\e[0;33m"
#define COLBLU "\e[0;34m"
#define COLMAG "\e[0;35m"
#define COLCYN "\e[0;36m"
#define COLWHT "\e[0;37m"

#define LOGINF(str, ...) logging::log_basic(COLRESET"[INFO] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGERR(str, ...) logging::log_basic(COLRED"[ERROR] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGWAR(str, ...) logging::log_basic(COLYEL"[WARN] %s: " str, __FUNCTION__, ## __VA_ARGS__)

namespace logging{
    void log_basic(const char* str, ...);
    void indent();
    void undent();
}

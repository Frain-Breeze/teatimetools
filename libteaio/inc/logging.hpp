#pragma once

#include <stddef.h>
#include <stdint.h>

//normal text colors
#ifdef TEA_ON_WINDOWS
#define COLBLK "\033[30m"
#define COLRED "\033[31m"
#define COLGRN "\033[32m"
#define COLYEL "\033[33m"
#define COLBLU "\033[34m"
#define COLMAG "\033[35m"
#define COLCYN "\033[36m"
#define COLWHT "\033[37m"
#else
#define COLBLK "\e[30m"
#define COLRED "\e[31m"
#define COLGRN "\e[32m"
#define COLYEL "\e[33m"
#define COLBLU "\e[34m"
#define COLMAG "\e[35m"
#define COLCYN "\e[36m"
#define COLWHT "\e[37m"
#endif

#define LOGALWAYS(str, ...) logging::log_basic(COLWHT"" str, ## __VA_ARGS__)
#define LOGOK(str, ...) logging::log_advanced(logging::LEVEL::Cok, COLGRN"[OK] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGVER(str, ...) logging::log_advanced(logging::LEVEL::Cverbose, COLBLU"[VERB] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGINF(str, ...) logging::log_advanced(logging::LEVEL::Cinfo, COLWHT"[INFO] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGERR(str, ...) logging::log_advanced(logging::LEVEL::Cerror, COLRED"[ERROR] %s: " str, __FUNCTION__, ## __VA_ARGS__)
#define LOGWAR(str, ...) logging::log_advanced(logging::LEVEL::Cwarning, COLYEL"[WARN] %s: " str, __FUNCTION__, ## __VA_ARGS__)

#define LOGNOK(str, name, ...) logging::log_advanced(logging::LEVEL::Cok, COLGRN"[OK] %s: " str, name, ## __VA_ARGS__)
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
		Cok,
		LEVEL_ITEM_COUNT
	};
	uint64_t count();
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

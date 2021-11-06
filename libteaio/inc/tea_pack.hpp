#pragma once

#ifdef __GNUC__
#define TEA_PACK_LIN __attribute__((__packed__))
#else
#define TEA_PACK_LIN
#endif

#ifdef _MSC_VER
#define TEA_PACK_WIN_START __pragma(pack(push, 1))
#define TEA_PACK_WIN_END __pragma(pack(pop))
#else
#define TEA_PACK_WIN_START
#define TEA_PACK_WIN_END
#endif
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)

project("k-on_PSP_tools")

#disable building of stuff we don't need
set(ENABLE_CPIO OFF)
set(ENABLE_CAT  OFF)
set(ENABLE_TAR  OFF)
set(ENABLE_LZMA OFF)
set(ENABLE_OPENSSL OFF)
#set(ENABLE_ZLIB OFF) #will cry on debug if turned off
set(ENABLE_BZip2 OFF)
set(ENABLE_LIBB2 OFF)
set(ENABLE_LZ4 OFF)
set(ENABLE_ZSTD OFF)
#set(ENABLE_LIBXML2 OFF) #needed for "xar"?
set(ENABLE_TEST OFF)
set(ENABLE_INSTALL OFF)
add_subdirectory("lib/libarchive/")

add_subdirectory("libteatime/")
add_subdirectory("teatimetools/")

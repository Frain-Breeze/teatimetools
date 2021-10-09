cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)

project("k-on_PSP_tools")

#disable building of applications we don't need
option(ENABLE_CPIO "" off)
option(ENABLE_CAT "" off)
option(ENABLE_TAR "" off)
add_subdirectory("lib/libarchive/")

add_subdirectory("libteatime/")
add_subdirectory("teatimetools/")

cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
project("k-on_PSP_tools")

#settings:
set(TEA_ENABLE_ISO on)
set(TEA_ENABLE_CPK on)


if(WIN32 OR MINGW)
    message("building for windows - good luck")
    add_compile_definitions(TEA_ON_WINDOWS)
	if(MINGW)
		link_libraries(-static-libgcc -static-libstdc++ -static)
	endif()
	
	set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/zlib_win/include/zlib/")
	
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ZLIB_LIBRARY "${CMAKE_SOURCE_DIR}/lib/zlib_win/lib/zlib_x64/zlibstatic.lib")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(ZLIB_LIBRARY "${CMAKE_SOURCE_DIR}/lib/zlib_win/lib/zlib_x86/zlibstatic.lib")
	endif()
	
else()
    message("building for linux")
    add_compile_definitions(TEA_ON_LINUX)
	set(CMAKE_CXX_FLAGS -gdwarf-3)
endif()

if(TEA_ENABLE_ISO)
    message("building with iso support")
    add_compile_definitions(TEA_ENABLE_ISO)

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
	set(ENABLE_WERROR OFF)
    add_subdirectory("lib/libarchive/")
endif()

if(TEA_ENABLE_CPK)
    message("building with cpk support")
    add_compile_definitions(TEA_ENABLE_CPK)
    add_subdirectory("libcpk/")
endif()

add_subdirectory("lib/zlib")
find_package(ZLIB REQUIRED)

add_subdirectory("libteaio/")
add_subdirectory("libteatime/")
add_subdirectory("teatimetools/")

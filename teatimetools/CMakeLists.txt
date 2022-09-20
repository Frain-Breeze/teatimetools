project("teatimetools")

add_executable("teatimetools"
	"src/main.cpp"
)

target_include_directories("teatimetools" PUBLIC
	"${CMAKE_SOURCE_DIR}/lib/libarchive/libarchive/"
	"${CMAKE_SOURCE_DIR}/lib/luawrapper/include/"
)

target_link_libraries("teatimetools"
	"libteatime"
	"lua_static"
	"boost-headers"
	$<$<BOOL:${TEA_ENABLE_ISO}>:archive_static> #libarchive
	$<$<BOOL:${TEA_ENABLE_CPK}>:cpk>
)

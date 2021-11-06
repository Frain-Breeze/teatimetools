project("teatimetools")

add_executable("teatimetools"
    "src/main.cpp"
)

target_link_libraries("teatimetools"
    "libteatime"
	$<$<BOOL:${WIN32}>:archive>
    $<$<BOOL:${TEA_ENABLE_ISO}>:archive_static> #libarchive
    $<$<BOOL:${TEA_ENABLE_CPK}>:cpk>
)

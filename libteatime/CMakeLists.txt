project("teatime")

add_library("libteatime" STATIC
    "src/fmdx.cpp"
	"src/uvr.cpp"
	"src/tts.cpp"
	"src/kmeans.cpp"
	"src/psp_dxt1.cpp"
	"src/chart.cpp"
	"src/vridge_obj.cpp"
	"src/digitalcute_bin.cpp"
)

target_link_libraries("libteatime" PUBLIC "wuquant")
target_link_libraries("libteatime" PUBLIC "teaio")
target_link_libraries("libteatime" PRIVATE ZLIB::ZLIB)

target_include_directories("libteatime" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_include_directories("libteatime" PUBLIC "${CMAKE_SOURCE_DIR}/lib/stb")

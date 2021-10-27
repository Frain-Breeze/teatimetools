project("teatime")

add_library("libteatime" STATIC
    "src/fmdx.cpp"
	"src/uvr.cpp"
	"src/tts.cpp"
	"src/kmeans.cpp"
)

target_link_libraries("libteatime" PUBLIC "teaio")

target_include_directories("libteatime" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_include_directories("libteatime" PUBLIC "${CMAKE_SOURCE_DIR}/lib/stb")

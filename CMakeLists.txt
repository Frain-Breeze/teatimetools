cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)

project("k-on_PSP_tools")

add_executable("kon-modder"
	"src/main.cpp"
	"src/fmdx.cpp"
	"src/uvr.cpp"
	"src/tts.cpp"
	"src/logging.cpp"
)
target_include_directories("kon-modder" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_include_directories("kon-modder" PRIVATE "${CMAKE_SOURCE_DIR}/lib/stb")

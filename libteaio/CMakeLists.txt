project("teaio") 

add_library("teaio"
    "src/logging.cpp"
    "src/reader.cpp"
    "src/reader_disk.cpp"
    "src/reader_memory.cpp"
)

target_include_directories("teaio" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc/")

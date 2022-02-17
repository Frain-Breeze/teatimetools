project("teaio") 

add_library("teaio" STATIC
    "src/logging.cpp"
    "src/reader.cpp"
    "src/reader_disk.cpp"
    "src/reader_memory.cpp"
    "src/reader_section.cpp"
)

target_include_directories("teaio" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc/")

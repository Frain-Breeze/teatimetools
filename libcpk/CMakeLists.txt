project("cpk")

add_library("cpk" STATIC
    "src/cpk.cpp"
)

target_link_libraries("cpk" PUBLIC "teaio")
target_include_directories("cpk" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc/")

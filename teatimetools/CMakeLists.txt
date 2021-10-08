project("teatimetools")

add_library("teatimetools"
    "src/main.cpp"
)

target_link_libraries("teatimetools" "libteatime")
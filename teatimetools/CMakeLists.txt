project("teatimetools")

add_executable("teatimetools"
    "src/main.cpp"
)

target_link_libraries("teatimetools" "libteatime")

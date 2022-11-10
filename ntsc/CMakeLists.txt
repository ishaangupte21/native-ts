set(CMAKE_CXX_STANDARD 17)
project(native-ts CXX)

include_directories("/usr/include/llvm-15")

add_subdirectory(lexer)
add_executable(ntsc main.cpp)

target_include_directories(ntsc PUBLIC
    "${CMAKE_SOURCE_DIR}/lexer"
    "${CMAKE_BINARY_DIR}/lexer"
)

target_link_libraries(ntsc PUBLIC
    lexer
)
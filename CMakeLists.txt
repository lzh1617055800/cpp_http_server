cmake_minimum_required(VERSION 3.10)

project(cpp_http_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(http_server ${SOURCES})
target_include_directories(http_server PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(http_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build
)

if(UNIX)
    target_link_libraries(http_server PRIVATE pthread)
endif()

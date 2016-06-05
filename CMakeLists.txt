cmake_minimum_required(VERSION 3.5)
project(Cpp_test)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories(/usr/include /usr/include/x86_64-linux-gnu)
set(SOURCE_FILES main.cpp)

add_subdirectory(addressbook)
include_directories(addressbook)
link_libraries(addressbook)



add_executable(Cpp_test ${SOURCE_FILES})
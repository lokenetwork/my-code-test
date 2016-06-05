cmake_minimum_required(VERSION 2.8)

project(addressbook)

file(GLOB HEADERS addressbook.pb.h)
set(SOURCES addressbook.pb.cc)

add_library(addressbook STATIC ${HEADERS} ${SOURCES})




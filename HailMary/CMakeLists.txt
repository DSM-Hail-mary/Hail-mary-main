cmake_minimum_required(VERSION 4.3)
project(HailMary)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(HailMary
        main.cpp
        main.h)

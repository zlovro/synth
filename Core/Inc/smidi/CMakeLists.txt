cmake_minimum_required(VERSION 3.28)
project(synth_midi C CXX)

set(CMAKE_C_STANDARD 23)

include_directories(include)
include_directories(include/synthinf)

add_executable(synth_midi main.cpp
        include/synthinf/serrno.c
        smidi.c
        smidi.h
        include/solfege/solfege.c)
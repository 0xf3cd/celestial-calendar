cmake_minimum_required(VERSION 3.22)

project(shared_lib VERSION 0.1.0)

set(SOURCES 
    lib_delta_t.cpp 
    lib_astro.cpp)

add_library(celestial_calendar SHARED ${SOURCES})

cmake_minimum_required(VERSION 3.22)

project(shared_lib VERSION 0.1.0)

# Find all source files to build the shared library
file(GLOB SOURCES "lib*.cpp")

add_library(celestial_calendar SHARED ${SOURCES})

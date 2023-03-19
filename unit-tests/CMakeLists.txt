cmake_minimum_required(VERSION 3.24)
project(unit_tests)

set(CMAKE_CXX_STANDARD 20)

add_executable(unit_tests
        catch.hpp
        tests_main.cpp
        tests.cpp)
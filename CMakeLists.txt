cmake_minimum_required(VERSION 3.24)
project(cpp_search_server)

set(CMAKE_CXX_STANDARD 20)

include_directories(search-server)

add_executable(cpp_search_server
        search-server/document.cpp
        search-server/document.h
        search-server/log_duration.h
        search-server/main.cpp
        search-server/paginator.h
        search-server/read_input_functions.cpp
        search-server/read_input_functions.h
        search-server/remove_duplicates.cpp
        search-server/remove_duplicates.h
        search-server/request_queue.cpp
        search-server/request_queue.h
        search-server/search_server.cpp
        search-server/search_server.h
        search-server/string_processing.cpp
        search-server/string_processing.h)

add_subdirectory(unit-tests)
cmake_minimum_required(VERSION 3.16)

set(VCPKG_MANIFEST_MODE ON)

project(
  "Assignment 2"
  VERSION 1.0
  LANGUAGES CXX)

find_package(Boost 1.79.0 REQUIRED COMPONENTS log program_options) 

add_library(rft SHARED librft/rft.cpp)
target_precompile_headers(rft PRIVATE librft/pch.hpp)
target_link_libraries(rft Boost::boost Boost::dynamic_linking Boost::log Boost::program_options)
target_compile_features(rft PUBLIC cxx_std_20)
set_target_properties(rft PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(client client/client.cpp)
target_precompile_headers(client PRIVATE client/pch.hpp)
target_link_libraries(client rft)
target_compile_features(client PUBLIC cxx_std_20)
set_target_properties(client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(server server/server.cpp)
target_precompile_headers(server PRIVATE server/pch.hpp)
target_link_libraries(server rft)
target_compile_features(server PUBLIC cxx_std_20)
set_target_properties(server PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
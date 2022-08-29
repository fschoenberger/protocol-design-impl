cmake_minimum_required(VERSION 3.16)

set(VCPKG_MANIFEST_MODE ON)
set(Boost_NO_WARN_NEW_VERSIONS 1)

project(
  "Assignment 2"
  VERSION 1.0
  LANGUAGES CXX)

find_package(Boost REQUIRED COMPONENTS program_options log log_setup) 
find_package(unofficial-hash-library CONFIG REQUIRED)

add_library(rft STATIC librft/pch.hpp librft/messages.hpp librft/exceptions.hpp "librft/logger.cpp" "librft/server.cpp" "librft/client.cpp")
target_precompile_headers(rft PRIVATE librft/pch.hpp)
target_link_libraries(rft PUBLIC Boost::boost Boost::dynamic_linking Boost::program_options Boost::log Boost::log_setup)
target_link_libraries(rft PRIVATE unofficial::hash-library)
target_compile_features(rft PUBLIC cxx_std_20)
target_compile_definitions(rft PUBLIC _WIN32_WINNT=0x0601)
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

add_executable(server "server/server.cpp")
target_precompile_headers(server PRIVATE server/pch.hpp)
target_link_libraries(server rft)
target_compile_features(server PUBLIC cxx_std_20)
set_target_properties(server PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
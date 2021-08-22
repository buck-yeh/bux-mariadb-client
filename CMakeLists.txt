cmake_minimum_required (VERSION 3.18)
cmake_policy(VERSION 3.18)
include(FetchContent)

FetchContent_Declare(
  FetchedBux
  GIT_REPOSITORY https://github.com/buck-yeh/bux.git
  GIT_TAG        main
  SOURCE_DIR     ../bux
)
FetchContent_MakeAvailable(FetchedBux)

project(bux-mariadb)

set(GNU_LIKE_CXX_FLAGS " -Wall -Wextra -Wshadow -Wconversion -Wno-parentheses -Wno-unknown-pragmas -g3 -Og -std=c++20")
#string(APPEND CMAKE_CXX_FLAGS " -g3")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message("Clang")
    string(APPEND CMAKE_CXX_FLAGS "${GNU_LIKE_CXX_FLAGS} -Wno-potentially-evaluated-expression")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message("GNU")
    string(APPEND CMAKE_CXX_FLAGS "${GNU_LIKE_CXX_FLAGS}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message("MSVC")
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /std:c++latest /MP")
else()
    message("Unknown compiler")
endif()

find_package(fmt CONFIG REQUIRED)

add_subdirectory (src)

install(TARGETS buxomariadb
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/bux DESTINATION include) # in ${CMAKE_INSTALL_PREFIX}/include/bux
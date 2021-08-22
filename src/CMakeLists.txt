add_library(buxomariadb STATIC oo_mariadb.cpp)
target_include_directories(buxomariadb PRIVATE ../include ../../bux/include)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
target_link_libraries(buxomariadb PRIVATE fmt::fmt)
endif()



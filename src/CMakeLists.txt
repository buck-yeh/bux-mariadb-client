add_library(bux-mariadb-client STATIC oo_mariadb.cpp)
#target_compile_options(bux-mariadb-client PRIVATE -DCLT_DEBUG_)
target_include_directories(bux-mariadb-client PRIVATE ../include)
if(NOT DEFINED FETCH_DEPENDEES)
    target_include_directories(bux-mariadb-client PRIVATE ../${DEPENDEE_ROOT}/bux/include)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
target_link_libraries(bux-mariadb-client PRIVATE fmt::fmt)
endif()

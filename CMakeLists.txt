cmake_minimum_required(VERSION 2.8)

PROJECT(RDMATEST)

set(CMAKE_BUILD_TYPE "Debug")

set(RDMA_LIB "-lrdmacm -libverbs -libumad -lpci -lefa")
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-pthread -ldl -lrt ${RDMA_LIB} -${CMAKE_CXX_FLAGS}")

if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wno-unused-variable")
else( CMAKE_BUILD_TYPE STREQUAL "Debug" )
        set(CMAKE_BUILD_TYPE "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fopenmp")
endif( CMAKE_BUILD_TYPE STREQUAL "Debug" )

message("Build Type: ${CMAKE_BUILD_TYPE} ${CMAKE_CXX_FLAGS}")

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(tool)
ADD_SUBDIRECTORY(src)

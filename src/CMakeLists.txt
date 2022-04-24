ADD_SUBDIRECTORY(include)

INCLUDE_DIRECTORIES(include)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/lib/perf)

set(PROC_SRC
	env_basic.cc
    rdma_com.cc
    mem_mag.cc)

ADD_EXECUTABLE(maintest main.cc)

ADD_EXECUTABLE(sendtest
            send_pro.cc
            ${PROC_SRC})
TARGET_LINK_LIBRARIES(sendtest perf)


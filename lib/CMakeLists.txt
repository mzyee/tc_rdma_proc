ADD_SUBDIRECTORY(perf)
set(PERF_SRC 
    perf/multicast_resources.c
    perf/perftest_communication.c
    perf/perftest_counters.c
    perf/perftest_parameters.c
    perf/perftest_resources.c
    perf/raw_ethernet_resources.c
    perf/get_clock.c)

ADD_LIBRARY(perf STATIC ${PERF_SRC})

# INSTALL(TARGETS perf perf
#       LIBRARY DESTINATION lib)

# INSTALL(FILES
#         perf/multicast_resources.h
# 		perf/perftest_communication.h
# 		perf/perftest_counters.h
# 		perf/perftest_parameters.h
# 		perf/perftest_resources.h
#         perf/raw_ethernet_resources.h
#         perf/get_clock.h
#         DESTINATION include/perf)

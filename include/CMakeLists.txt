SET(RES_PATH  "${PROJECT_SOURCE_DIR}/data" CACHE PATH "root path")
configure_file (
      "${PROJECT_SOURCE_DIR}/include/project_dir.h.in"
      "${PROJECT_SOURCE_DIR}/include/project_dir.h"
      )
message("CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
message("PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}")


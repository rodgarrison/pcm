set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/example1)

file(GLOB EXAMPLE_FILE example1.cpp)

add_executable(example1 ${EXAMPLE_FILE})
add_dependencies(example1 PCM_SHARED)
set_target_properties(example1 PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH_USE_LINK_PATH TRUE
  INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib:${INSTALL_RPATH}")   # rpath to libpcm.so
target_compile_definitions(example1 PRIVATE PCM_DYNAMIC_LIB) # -DPCM_DYNAMIC_LIB
target_link_libraries(example1 PRIVATE Threads::Threads dl)  # -pthread -ldl

add_executable(example1_shlib ${EXAMPLE_FILE})
target_link_libraries(example1_shlib PUBLIC PCM_SHARED PRIVATE Threads::Threads dl)

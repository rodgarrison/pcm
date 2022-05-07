set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/example2)
set(CMAKE_CXX_STANDARD 17)

file(GLOB EXAMPLE_FILE example2.cpp)

add_executable(example2 ${EXAMPLE_FILE})
add_dependencies(example2 PCM_SHARED)
set_target_properties(example2 PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH_USE_LINK_PATH TRUE
  INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib:${INSTALL_RPATH}")   # rpath to libpcm.so
target_compile_definitions(example2 PRIVATE PCM_DYNAMIC_LIB) # -DPCM_DYNAMIC_LIB
target_include_directories(example2 PUBLIC .)
target_compile_options(example2 PUBLIC "-fno-diagnostics-color")
target_link_libraries(example2 PRIVATE Threads::Threads dl)  # -pthread -ldl

add_executable(example2_shlib ${EXAMPLE_FILE})
target_include_directories(example2_shlib PUBLIC .)
target_compile_options(example2_shlib PUBLIC "-fno-diagnostics-color")
target_link_libraries(example2_shlib PUBLIC PCM_SHARED PRIVATE Threads::Threads dl)

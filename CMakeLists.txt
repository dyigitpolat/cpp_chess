cmake_minimum_required(VERSION 3.16)
project(cpp_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(chess_ga
  src/main.cpp
  src/board.cpp
  src/mate.cpp
  src/ga.cpp
  src/html_export.cpp
  src/local_search.cpp
)

add_executable(bench_ga
  src/bench_main.cpp
  src/board.cpp
  src/mate.cpp
  src/ga.cpp
  src/html_export.cpp
  src/local_search.cpp
)

add_executable(bench_mate
  src/bench_mate.cpp
  src/board.cpp
  src/mate.cpp
)

target_include_directories(chess_ga PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(bench_ga PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(bench_mate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
  "Debug" "Release" "RelWithDebInfo" "MinSizeRel" "Profile")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_err)
# LTO obscures perf symbols; skip for Profile builds.
if(ipo_supported AND NOT CMAKE_BUILD_TYPE STREQUAL "Profile")
  set_property(TARGET chess_ga PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  set_property(TARGET bench_ga PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Optional PGO (manual): build with -fprofile-generate, run a training workload, rebuild with -fprofile-use.

# Optional OpenMP for parallel fitness evaluation
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(chess_ga PRIVATE OpenMP::OpenMP_CXX)
  target_link_libraries(bench_ga PRIVATE OpenMP::OpenMP_CXX)
endif()

# Profile: optimized + debug symbols + frame pointers for perf (no LTO — see above).
target_compile_options(chess_ga PRIVATE
  $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:-O3>
  $<$<CONFIG:Profile>:-g>
  $<$<CONFIG:Profile>:-fno-omit-frame-pointer>
  $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
)
target_compile_options(bench_ga PRIVATE
  $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:-O3>
  $<$<CONFIG:Profile>:-g>
  $<$<CONFIG:Profile>:-fno-omit-frame-pointer>
  $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
)
target_compile_options(bench_mate PRIVATE
  $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:-O3>
  $<$<CONFIG:Profile>:-g>
  $<$<CONFIG:Profile>:-fno-omit-frame-pointer>
  $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
)

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
)

target_include_directories(chess_ga PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Optional OpenMP for parallel fitness evaluation
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(chess_ga PRIVATE OpenMP::OpenMP_CXX)
endif()

target_compile_options(chess_ga PRIVATE
  $<$<CONFIG:Release>:-O3>
)

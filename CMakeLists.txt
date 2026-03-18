cmake_minimum_required(VERSION 3.24)

project(case LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

add_library(case SHARED)
target_link_libraries(case PUBLIC ${BLAS_LIBRARIES})

target_include_directories(case PUBLIC
  $<BUILD_INTERFACE: ${CMAKE_CURRENT_LIST_DIR}>
  $<BUILD_INTERFACE: ${CMAKE_CURRENT_LIST_DIR}/src>
  $<INSTALL_INTERFACE:include>
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/apps)

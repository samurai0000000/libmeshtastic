# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.0)

project(libmeshtastic)

find_package(Protobuf REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-g -O2 -I${CMAKE_CURRENT_LIST_DIR})
file(GLOB MESHTASTIC_PROTOS_SRCS "Meshtastic-arduino/src/meshtastic/*.c")
include_directories(${CMAKE_CURRENT_LIST_DIR}/Meshtastic-arduino/src)
set(ARDUINO_SRCS
  Meshtastic-arduino/src/pb_common.c
  Meshtastic-arduino/src/pb_encode.c
  Meshtastic-arduino/src/pb_decode.c
  ${MESHTASTIC_PROTOS_SRCS}
  )
set(LIBMESHTASTIC_SRCS
  serial-posix.c
  protocol.c
  MeshPrint.cxx
  MeshClient.cxx
  )
add_library(libmeshtastic ${LIBMESHTASTIC_SRCS} ${ARDUINO_SRCS})
target_include_directories(libmeshtastic PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Meshtastic-arduino/src
  )

add_executable(mynodeid sample/mynodeid.c)
target_link_libraries(mynodeid PRIVATE libmeshtastic protobuf-c)

add_executable(nodelisten sample/nodelisten.c)
target_link_libraries(nodelisten PRIVATE libmeshtastic protobuf-c)

add_executable(nodereboot sample/nodereboot.c)
target_link_libraries(nodereboot PRIVATE libmeshtastic protobuf-c)

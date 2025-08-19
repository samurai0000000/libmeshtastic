# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.0)

project(libmeshtastic)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-g -O2 -I${CMAKE_CURRENT_LIST_DIR})

file(GLOB MESHTASTIC_PROTOS_SRCS "Meshtastic-arduino/src/meshtastic/*.c")
include_directories(${CMAKE_CURRENT_LIST_DIR}/Meshtastic-arduino/src)

set(ARDUINO_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/Meshtastic-arduino/src/pb_common.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Meshtastic-arduino/src/pb_encode.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Meshtastic-arduino/src/pb_decode.c
  ${MESHTASTIC_PROTOS_SRCS}
  )

if (DEFINED PICO_PLATFORM)
  set(LIBMESHTASTIC_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/serial-pico.c
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SimpleClient.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/HomeChat.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/BaseNvm.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/SimpleShell.cxx
    )
else ()
  FIND_PACKAGE(config++ QUIET)
  if (NOT config++_FOUND)
    find_library(CONFIG++_LIBRARY NAMES config++)
  endif ()
  set(LIBMESHTASTIC_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/serial-posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshPrint.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/SimpleClient.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshClient.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/HomeChat.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/BaseNvm.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshNvm.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/SimpleShell.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshShell.cxx
    )
endif ()

add_library(libmeshtastic INTERFACE)
target_include_directories(libmeshtastic INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Meshtastic-arduino/src
  )
target_sources(libmeshtastic INTERFACE
  ${LIBMESHTASTIC_SRCS}
  ${ARDUINO_SRCS}
  )

if (NOT DEFINED PICO_PLATFORM AND NOT CONFIG++_LIBRARY-NOTFOUND)
  add_executable(mynodeid sample/mynodeid.c)
  target_link_libraries(mynodeid PUBLIC libmeshtastic ${CONFIG++_LIBRARY})

  add_executable(nodelisten sample/nodelisten.c)
  target_link_libraries(nodelisten PUBLIC libmeshtastic ${CONFIG++_LIBRARY})

  add_executable(nodereboot sample/nodereboot.c)
  target_link_libraries(nodereboot PUBLIC libmeshtastic ${CONFIG++_LIBRARY})
endif ()

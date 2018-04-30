project(ice VERSION 0.1.0 LANGUAGES CXX)

file(GLOB_RECURSE headers include/ice/*.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/ice PREFIX include FILES ${headers})

file(GLOB_RECURSE sources src/*.h src/*.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX src FILES ${sources})

try_compile(CHECK_RESULT ${CMAKE_CURRENT_BINARY_DIR}
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/res/check.cpp
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  OUTPUT_VARIABLE CHECK_OUTPUT)

string(REPLACE "\n" ";" CHECK_OUTPUT "${CHECK_OUTPUT}")
set(CHECK_REGEX ".*check<([^,>]+), ?([0-9]+), ?([0-9]+)>.*")
foreach(CHECK_LINE ${CHECK_OUTPUT})
  if(CHECK_LINE MATCHES "${CHECK_REGEX}")
    string(REGEX REPLACE "${CHECK_REGEX}" "\\1;\\2;\\3" CHECK_ENTRY ${CHECK_LINE})
    list(GET CHECK_ENTRY 0 CHECK_TYPE)
    if(NOT ${CHECK_TYPE}_size AND NOT ${CHECK_TYPE}_alignment)
      list(GET CHECK_ENTRY 1 ${CHECK_TYPE}_size)
      list(GET CHECK_ENTRY 2 ${CHECK_TYPE}_alignment)
      message(STATUS "Checking ${CHECK_TYPE}: ${${CHECK_TYPE}_size} ${${CHECK_TYPE}_alignment}")
    endif()
  endif()
endforeach()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ice/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/ice/config.h
  NEWLINE_STYLE LF)

source_group(include FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ice/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/ice/config.h)

list(APPEND headers
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ice/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/ice/config.h)

add_library(${PROJECT_NAME} STATIC ${headers} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF FOLDER ice)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(WIN32)
  target_compile_options(${PROJECT_NAME} PUBLIC /await)
else()
  target_compile_options(${PROJECT_NAME} PUBLIC -fcoroutines-ts)
endif()

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32 mswsock)
endif()

install(TARGETS ${PROJECT_NAME} EXPORT ice LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h")

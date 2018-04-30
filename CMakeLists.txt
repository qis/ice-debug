cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(ice VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_EXAMPLES "Build examples" OFF)
option(EXTRA_WARNINGS "Build with additional warnings" OFF)

if(MSVC)
  add_definitions(-D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_ATL_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=_CPPUNWIND)
  add_definitions(-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
endif()

if(EXTRA_WARNINGS)
  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
    string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-absolute-paths -fcolor-diagnostics")
  endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER build)

add_subdirectory(src/ice)

include(CMakePackageConfigHelpers)

configure_package_config_file(src/config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
  INSTALL_DESTINATION lib/cmake/${PROJECT_NAME})

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake DESTINATION lib/cmake/${PROJECT_NAME})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION lib/cmake/${PROJECT_NAME})
install(EXPORT ice FILE ${PROJECT_NAME}-targets.cmake NAMESPACE ice:: DESTINATION lib/cmake/${PROJECT_NAME})

if(BUILD_EXAMPLES)
  add_executable(main src/main.cpp)
  source_group("" FILES src/main.cpp)
  target_link_libraries(main PRIVATE ice)
  set_target_properties(main PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT main)
endif()

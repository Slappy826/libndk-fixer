cmake_minimum_required(VERSION 3.22)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

set(ANDROID_PLATFORM 24)
set(ANDROID_ABI x86_64)
set(ANDROID_STL c++_static)
set(ANDROID_CPP_FEATURES, "no-rtti no-exceptions")
set(CMAKE_TOOLCHAIN_FILE $ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake)

project(ndk_fixer)

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 24)
set(CMAKE_ANDROID_ARCH_ABI x86_64)
set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK_HOME})
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
set(CMAKE_ANDROID_STL_TYPE c++_static)
set(CMAKE_CXX_STANDARD 20)

if(NOT TARGET dobby)
set(DOBBY_DIR dobby)
macro(SET_OPTION option value)
  set(${option} ${value} CACHE INTERNAL "" FORCE)
endmacro()
SET_OPTION(DOBBY_DEBUG OFF)
SET_OPTION(DOBBY_GENERATE_SHARED OFF)
add_subdirectory(${DOBBY_DIR} dobby)
get_property(DOBBY_INCLUDE_DIRECTORIES
  TARGET dobby
  PROPERTY INCLUDE_DIRECTORIES)
include_directories(
  .
  ${DOBBY_INCLUDE_DIRECTORIES}
  $<TARGET_PROPERTY:dobby,INCLUDE_DIRECTORIES>
)
endif()

add_library(ndk_fixer SHARED src/main.cpp)
target_link_libraries(ndk_fixer log dobby_static)
add_custom_command(
    TARGET "ndk_fixer" POST_BUILD
    DEPENDS "ndk_fixer"
    COMMAND $<$<CONFIG:release>:${CMAKE_STRIP}>
    ARGS --strip-all $<TARGET_FILE:ndk_fixer>
)

install(TARGETS ndk_fixer
        LIBRARY DESTINATION lib)

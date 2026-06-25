cmake_minimum_required(VERSION 3.24)

project(VibeReady VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "VibeReady MVP client builds only on Windows x64.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "VibeReady MVP client supports only Windows x64 builds.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(VibeReady WIN32
  src/windows/main.cpp
)

target_compile_definitions(VibeReady PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  WINVER=0x0A00
  _WIN32_WINNT=0x0A00
)

if(MSVC)
  target_compile_options(VibeReady PRIVATE /W4 /permissive-)
else()
  target_compile_options(VibeReady PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS VibeReady RUNTIME DESTINATION .)
install(FILES README.txt THIRD-PARTY-NOTICES.txt DESTINATION .)

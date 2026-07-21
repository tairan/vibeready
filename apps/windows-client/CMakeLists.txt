if(NOT WIN32)
  message(FATAL_ERROR "VibeReady MVP client builds only on Windows x64.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "VibeReady MVP client supports only Windows x64 builds.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VIBEREADY_VERSION_TWEAK 0)
set(VIBEREADY_TELEMETRY_ENDPOINT "" CACHE STRING "Production telemetry batch endpoint URL.")

set(VIBEREADY_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/version.h.in "${VIBEREADY_GENERATED_DIR}/version.h" @ONLY)
configure_file(src/VibeReady.rc.in "${VIBEREADY_GENERATED_DIR}/VibeReady.rc" @ONLY)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(VibeReady WIN32
  src/main.cpp
  src/telemetry.cpp
  src/ui_foundation.cpp
  "${VIBEREADY_GENERATED_DIR}/VibeReady.rc"
)

set_target_properties(VibeReady PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
if(CMAKE_CONFIGURATION_TYPES)
  foreach(VIBEREADY_CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${VIBEREADY_CONFIG}" VIBEREADY_CONFIG_UPPER)
    set_target_properties(VibeReady PROPERTIES
      "RUNTIME_OUTPUT_DIRECTORY_${VIBEREADY_CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/${VIBEREADY_CONFIG}"
    )
  endforeach()
endif()

target_link_libraries(VibeReady PRIVATE
  d2d1
  dwrite
  ole32
  winhttp
)

target_include_directories(VibeReady PRIVATE
  "${VIBEREADY_GENERATED_DIR}"
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
  target_compile_options(VibeReady PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(VibeReady PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(VibeReadyTelemetryTests
    tests/telemetry_contract.cpp
    src/telemetry.cpp
  )
  set_target_properties(VibeReadyTelemetryTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  if(CMAKE_CONFIGURATION_TYPES)
    foreach(VIBEREADY_CONFIG Debug Release RelWithDebInfo MinSizeRel)
      string(TOUPPER "${VIBEREADY_CONFIG}" VIBEREADY_CONFIG_UPPER)
      set_target_properties(VibeReadyTelemetryTests PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${VIBEREADY_CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/${VIBEREADY_CONFIG}"
      )
    endforeach()
  endif()
  target_include_directories(VibeReadyTelemetryTests PRIVATE src)
  target_link_libraries(VibeReadyTelemetryTests PRIVATE ole32 winhttp)
  target_compile_definitions(VibeReadyTelemetryTests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
  )
  if(MSVC)
    target_compile_options(VibeReadyTelemetryTests PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(VibeReadyTelemetryTests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME VibeReadyTelemetryContract COMMAND VibeReadyTelemetryTests)
endif()

install(TARGETS VibeReady RUNTIME DESTINATION .)
install(FILES README.txt THIRD-PARTY-NOTICES.txt DESTINATION .)

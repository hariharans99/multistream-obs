cmake_minimum_required(VERSION 3.28...3.30)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define project name and version first (read from buildspec.json or passed)
# But bootstrap.cmake usually does this.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(SKIP_DEPENDENCIES "Skip dependency downloading/building" OFF)

include(compilerconfig)

# defaults.cmake includes buildspec.cmake which does the dependency setup.
if(NOT SKIP_DEPENDENCIES)
  include(defaults)
else()
  # If skipping dependencies, we still need some of the logic from defaults
  set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
endif()

include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/multistream-output.cpp
    src/multistream-output.h
    src/stream-target.cpp
    src/stream-target.h
    src/stream-config.h
    src/settings-ui.cpp
    src/settings-ui.h
    src/util/hw-capability.cpp
    src/util/hw-capability.h
    src/util/logger.cpp
    src/util/logger.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}/.deps/include")

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# ── OBS linkage ───────────────────────────────────────────────────────────────
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_FRONTEND_API)
endif()

# ── Qt linkage ────────────────────────────────────────────────────────────────
if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Gui)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# ── Windows-specific ──────────────────────────────────────────────────────────
if(WIN32)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

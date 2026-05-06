# CMake Windows build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# _check_dependencies_windows: Set up Windows slice for _check_dependencies
function(_check_dependencies_windows)
  # CMAKE_VS_PLATFORM_NAME is only set by the Visual Studio generator.
  # With Ninja + MSVC (VS Build Tools), we must derive the arch manually.
  if(CMAKE_VS_PLATFORM_NAME)
    set(arch ${CMAKE_VS_PLATFORM_NAME})
  elseif(CMAKE_GENERATOR_PLATFORM)
    set(arch ${CMAKE_GENERATOR_PLATFORM})
  else()
    # Derive from the compiler target: MSVC sets CMAKE_SYSTEM_PROCESSOR
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(arch x64)
    else()
      set(arch x86)
    endif()
  endif()
  set(platform windows-${arch})

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.zip")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()
endfunction()

_check_dependencies_windows()

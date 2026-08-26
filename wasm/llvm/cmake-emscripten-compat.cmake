# Emscripten/LLVM 16 compatibility shim.
#
# LLVM's config-ix.cmake manually includes Platform/${CMAKE_HOST_SYSTEM_NAME}
# while cross-compiling in order to discover host filename suffixes. CMake's
# UnixPaths.cmake expects _cmake_record_install_prefix(), which is normally
# defined by CMakeSystemSpecificInformation.cmake before normal platform setup.
# That ordering is absent on this manual host-platform include. Define the
# official CMake 3.28 helper here before LLVM's project() runs.
if(NOT COMMAND _cmake_record_install_prefix)
  function(_cmake_record_install_prefix)
    set(_CMAKE_SYSTEM_PREFIX_PATH_INSTALL_PREFIX_VALUE "${CMAKE_INSTALL_PREFIX}" PARENT_SCOPE)
    set(_CMAKE_SYSTEM_PREFIX_PATH_STAGING_PREFIX_VALUE "${CMAKE_STAGING_PREFIX}" PARENT_SCOPE)
    set(icount 0)
    set(scount 0)
    foreach(value IN LISTS CMAKE_SYSTEM_PREFIX_PATH)
      if(value STREQUAL CMAKE_INSTALL_PREFIX)
        math(EXPR icount "${icount}+1")
      endif()
      if(value STREQUAL CMAKE_STAGING_PREFIX)
        math(EXPR scount "${scount}+1")
      endif()
    endforeach()
    set(_CMAKE_SYSTEM_PREFIX_PATH_INSTALL_PREFIX_COUNT "${icount}" PARENT_SCOPE)
    set(_CMAKE_SYSTEM_PREFIX_PATH_STAGING_PREFIX_COUNT "${scount}" PARENT_SCOPE)
  endfunction()
endif()

# FindGLFW3
# -------------
#
# Finds the GLFW library. This module defines:
#
#  GLFW_FOUND           - True if GLFW library is found
#  GLFW::GLFW           - GLFW imported target
#
# Additionally these variables are defined for internal usage:
#
#  GLFW_LIBRARY         - GLFW library
#  GLFW_LIBRARIES       - Same as GLFW_LIBRARY
#  GLFW_INCLUDE_DIR     - Include dir
#

# Include dir
find_path(GLFW_INCLUDE_DIR
  NAMES
    glfw3.h
  PATH_SUFFIXES
    include/GLFW
    include
    GLFW)

# Library
find_library(GLFW_LIBRARY
  NAMES
    glfw
  PATH_SUFFIXES
    lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW DEFAULT_MSG
  GLFW_LIBRARY
  GLFW_INCLUDE_DIR)

mark_as_advanced(FORCE
  GLFW_LIBRARY
  GLFW_INCLUDE_DIR)

if(GLFW_FOUND AND NOT TARGET GLFW::GLFW)
  add_library(GLFW::GLFW UNKNOWN IMPORTED)
  set_target_properties(GLFW::GLFW PROPERTIES
    IMPORTED_LOCATION ${GLFW_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${GLFW_INCLUDE_DIR})
endif()

set(GLFW_LIBRARIES ${GLFW_LIBRARY})
set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIR})


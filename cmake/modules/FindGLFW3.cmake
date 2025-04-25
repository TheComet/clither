# FindGLFW3
# -------------
#
# Finds the GLFW3 library. This module defines:
#
#  GLFW3_FOUND          - True if GLFW3 library is found
#  GLFW3::GLFW3         - GLFW3 imported target
#
# Additionally these variables are defined for internal usage:
#
#  GLFW3_LIBRARY        - GLFW library
#  GLFW3_LIBRARIES      - Same as GLFW_LIBRARY
#  GLFW3_INCLUDE_DIR    - Include dir
#

# Include dir
find_path(GLFW3_INCLUDE_DIR
  NAMES
    glfw3.h
  PATH_SUFFIXES
    include/GLFW
    include
    GLFW)

# Library
find_library(GLFW3_LIBRARY
  NAMES
    glfw
  PATH_SUFFIXES
    lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW3 DEFAULT_MSG
  GLFW3_LIBRARY
  GLFW3_INCLUDE_DIR)

mark_as_advanced(FORCE
  GLFW3_LIBRARY
  GLFW3_INCLUDE_DIR)

if(GLFW3_FOUND AND NOT TARGET GLFW3::GLFW3)
  add_library(GLFW3::GLFW3 UNKNOWN IMPORTED)
  set_target_properties(GLFW3::GLFW3 PROPERTIES
    IMPORTED_LOCATION ${GLFW3_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${GLFW3_INCLUDE_DIR})
endif()

set(GLFW3_LIBRARIES ${GLFW3_LIBRARY})
set(GLFW3_INCLUDE_DIRS ${GLFW3_INCLUDE_DIR})


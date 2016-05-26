
#
# Searching for Glib resource compiler.
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

#
# Generate resource C file.
file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
execute_process (COMMAND ${RESOURCE_COMPILER}
                         --generate-source
                         --target ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c
                         resources.xml
                         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

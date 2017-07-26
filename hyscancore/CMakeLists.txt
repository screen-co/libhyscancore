
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
add_custom_command (OUTPUT ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c
                    COMMAND ${RESOURCE_COMPILER} --generate-source
                            --target ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c
                            hyscan-core-resources.xml
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    DEPENDS hyscan-core-resources.xml
                    DEPENDS project-schema.xml
                    DEPENDS track-schema.xml
                    VERBATIM)

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/marshallers)
include_directories (${CMAKE_BINARY_DIR}/marshallers)
add_custom_command (OUTPUT ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c
                    COMMAND ${MARSHAL_COMPILER} --header
                            --prefix hyscan_core_marshal
                            hyscan-core-marshallers.list >
                            ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.h
                    COMMAND ${MARSHAL_COMPILER} --body
                            --prefix hyscan_core_marshal
                            hyscan-core-marshallers.list >
                            ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS hyscan-core-marshallers.list
                    VERBATIM)

add_library (hyscancore SHARED
             hyscan-core-types.c
             hyscan-data-writer.c
             hyscan-raw-data.c
             hyscan-acoustic-data.c
             hyscan-nmea-data.c
             hyscan-forward-look-data.c
             hyscan-forward-look-player.c
             hyscan-geo.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c)

target_link_libraries (hyscancore ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (hyscancore PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")

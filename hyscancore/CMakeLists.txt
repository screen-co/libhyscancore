
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/resources")

add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c"
                    COMMAND ${RESOURCE_COMPILER} --generate-source
                            --target "${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c"
                            hyscan-core-resources.xml
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    DEPENDS hyscan-core-resources.xml
                    DEPENDS project-schema.xml
                    DEPENDS track-schema.xml
                    VERBATIM)

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/marshallers")
include_directories ("${CMAKE_BINARY_DIR}/marshallers")

add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c"
                    COMMAND ${MARSHAL_COMPILER} --header
                            --prefix hyscan_core_marshal
                            hyscan-core-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.h"
                    COMMAND ${MARSHAL_COMPILER} --body
                            --prefix hyscan_core_marshal
                            hyscan-core-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS hyscan-core-marshallers.list
                    VERBATIM)

add_library (${HYSCAN_CORE_LIBRARY} SHARED
             hyscan-core-common.c
             hyscan-data-writer.c
             hyscan-amplitude.c
             hyscan-acoustic-data.c
             hyscan-nmea-data.c
             hyscan-forward-look-data.c
             hyscan-forward-look-player.c
             hyscan-geo.c
             hyscan-nav-data.c
             hyscan-depthometer.c
             hyscan-control.c
             hyscan-control-proxy.c
             hyscan-profile.c
             hyscan-profile-db.c
             hyscan-profile-offset.c
             hyscan-profile-hw-device.c
             hyscan-profile-hw.c
             hyscan-task-queue.c
             hyscan-mark.c
             hyscan-object.c
             hyscan-object-store.c
             hyscan-object-data.c
             hyscan-object-data-wfmark.c
             hyscan-object-data-geomark.c
             hyscan-planner.c
             hyscan-object-data-planner.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c)

target_link_libraries (${HYSCAN_CORE_LIBRARY} ${GLIB2_LIBRARIES} ${MATH_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (${HYSCAN_CORE_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_CORE_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_CORE_VERSION})

configure_file (hyscancore.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancore-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_CORE_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancore-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES hyscan-core-schemas.h
               hyscan-data-writer.h
               hyscan-amplitude.h
               hyscan-acoustic-data.h
               hyscan-nmea-data.h
               hyscan-forward-look-data.h
               hyscan-forward-look-player.h
               hyscan-geo.h
               hyscan-nav-data.h
               hyscan-depthometer.h
               hyscan-control.h
               hyscan-control-proxy.h
               hyscan-profile.h
               hyscan-profile-db.h
               hyscan-profile-offset.h
               hyscan-profile-hw-device.h
               hyscan-profile-hw.h
               hyscan-task-queue.h
               hyscan-mark.h
               hyscan-object.h
               hyscan-object-data.h
               hyscan-object-data-wfmark.h
               hyscan-object-data-geomark.h
               hyscan-planner.h
               hyscan-object-data-planner.h
         COMPONENT development
         DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/hyscan-${HYSCAN_MAJOR_VERSION}/hyscancore"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

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
             hyscan-amplitude-factory.c
             hyscan-nmea-data.c
             hyscan-forward-look-data.c
             hyscan-forward-look-player.c
             hyscan-geo.c
             hyscan-nav-data.c
             hyscan-nmea-parser.c
             hyscan-depthometer.c
             hyscan-depth-factory.c
             hyscan-control.c
             hyscan-control-proxy.c
             hyscan-projector.c
             hyscan-tile-common.c
             hyscan-tile-color.c
             hyscan-waterfall-tile.c
             hyscan-tile-queue.c
             hyscan-track-rect.c
             hyscan-mark.c
             hyscan-mark-data.c
             hyscan-mark-data-waterfall.c
             hyscan-mark-data-geo.c
             hyscan-mark-model.c
             hyscan-mloc.c
             hyscan-hw-connector.c
             hyscan-profile.c
             hyscan-profile-db.c
             hyscan-profile-hw-device.c
             hyscan-profile-hw.c
             hyscan-profile-offset.c
             hyscan-geo-projection.c
             hyscan-mercator.c
             hyscan-pseudo-mercator.c
             hyscan-cartesian.c
             hyscan-task-queue.c
             hyscan-list-model.c
             hyscan-planner.c
             hyscan-nav-model.c
             hyscan-nmea-file-device.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c)

target_link_libraries (${HYSCAN_CORE_LIBRARY}
                       ${GLIB2_LIBRARIES}
                       ${MATH_LIBRARIES}
                       ${HYSCAN_LIBRARIES}
                       ${ZLIB_LIBRARIES})

set_target_properties (${HYSCAN_CORE_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_CORE_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_CORE_VERSION})

configure_file (hyscancore.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancore-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_CORE_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancore-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "lib/pkgconfig"
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
               hyscan-nmea-parser.h
               hyscan-depthometer.h
               hyscan-control.h
               hyscan-control-proxy.h
               hyscan-amplitude-factory.h
               hyscan-depth-factory.h
               hyscan-projector.h
               hyscan-track-rect.h
               hyscan-mloc.h
               hyscan-tile-common.h
               hyscan-tile-queue.h
               hyscan-tile-color.h
               hyscan-waterfall-tile.h
               hyscan-mark.h
               hyscan-mark-data.h
               hyscan-mark-data-waterfall.h
               hyscan-mark-data-geo.h
               hyscan-mark-model.h
               hyscan-hw-connector.h
               hyscan-profile.h
               hyscan-profile-db.h
               hyscan-profile-hw-device.h
               hyscan-profile-hw.h
               hyscan-profile-offset.h
               hyscan-geo-projection.h
               hyscan-mercator.h
               hyscan-pseudo-mercator.h
               hyscan-cartesian.h
               hyscan-task-queue.h
               hyscan-list-model.h
               hyscan-planner.h
        hyscan-nav-model.h
               hyscan-nmea-file-device.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscancore"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

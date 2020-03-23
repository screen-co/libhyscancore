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
             hyscan-factory.c
             hyscan-factory-amplitude.c
             hyscan-factory-depth.c
             hyscan-nmea-data.c
             hyscan-forward-look-data.c
             hyscan-forward-look-player.c
             hyscan-geo.c
             hyscan-nav-data.c
             hyscan-nav-smooth.c
             # hyscan-nav-filter.c
             hyscan-nmea-parser.c
             hyscan-depthometer.c
             hyscan-control.c
             hyscan-control-proxy.c
             hyscan-projector.c
             hyscan-tile.c
             hyscan-tile-color.c
             hyscan-waterfall-tile.c
             hyscan-tile-queue.c
             hyscan-track-rect.c
             hyscan-mark.c
             hyscan-label.c
             hyscan-track-data.c
             hyscan-object-data.c
             hyscan-object-data-wfmark.c
             hyscan-object-data-geomark.c
             hyscan-object-data-label.c
             hyscan-object-model.c
             hyscan-mloc.c
             hyscan-hw-connector.c
             hyscan-profile.c
             hyscan-profile-db.c
             hyscan-profile-hw-device.c
             hyscan-profile-hw.c
             hyscan-task-queue.c
             hyscan-profile-offset.c
             hyscan-geo-projection.c
             hyscan-proj.c
             hyscan-cartesian.c
             hyscan-planner.c
             hyscan-planner-model.c
             hyscan-object-data-planner.c
             hyscan-nav-model.c
             hyscan-nmea-file-device.c
             hyscan-data-player.c
             hyscan-hsx-converter.c
             ${CMAKE_BINARY_DIR}/marshallers/hyscan-core-marshallers.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c)

target_link_libraries (${HYSCAN_CORE_LIBRARY}
                       ${GLIB2_LIBRARIES}
                       ${MATH_LIBRARIES}
                       ${HYSCAN_LIBRARIES}
                       ${ZLIB_LIBRARIES}
                       ${PROJ4_LIBRARIES})

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
               hyscan-nav-smooth.h
               hyscan-nmea-parser.h
               hyscan-depthometer.h
               hyscan-control.h
               hyscan-control-proxy.h
               hyscan-factory-amplitude.h
               hyscan-factory-depth.h
               hyscan-projector.h
               hyscan-track-rect.h
               hyscan-mloc.h
               hyscan-tile.h
               hyscan-tile-queue.h
               hyscan-tile-color.h
               hyscan-waterfall-tile.h
               hyscan-mark.h
               hyscan-label.h
               hyscan-track-data.h
               hyscan-object-data.h
               hyscan-object-data-wfmark.h
               hyscan-object-data-geomark.h
               hyscan-object-data-label.h
               hyscan-object-model.h
               hyscan-hw-connector.h
               hyscan-profile.h
               hyscan-profile-db.h
               hyscan-profile-hw-device.h
               hyscan-profile-hw.h
               hyscan-task-queue.h
               hyscan-profile-offset.h
               hyscan-geo-projection.h
               hyscan-proj.h
               hyscan-cartesian.h
               hyscan-planner.h
               hyscan-planner-model.h
               hyscan-object-data-planner.h
               hyscan-nav-model.h
               hyscan-nmea-file-device.h
               hyscan-data-player.h
               hyscan-hsx-converter.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscancore"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

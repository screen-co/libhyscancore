
find_program (RESOURCE_COMPILER glib-compile-resources)
if (${RESOURCE_COMPILER} STREQUAL RESOURCE_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib resource compiler not found.")
endif ()

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
add_custom_command (OUTPUT ${CMAKE_BINARY_DIR}/resources/hyscan-location-test-resources.c
                    COMMAND ${RESOURCE_COMPILER} --generate-source
                            --target ${CMAKE_BINARY_DIR}/resources/hyscan-location-test-resources.c
                            location-test-resources.xml
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    DEPENDS location-test-resources.xml
                    VERBATIM)

add_executable (data-writer-test data-writer-test.c)
add_executable (acoustic-data-test acoustic-data-test.c)
add_executable (forward-look-data-test forward-look-data-test.c)
add_executable (location-test location-test.c ${CMAKE_BINARY_DIR}/resources/hyscan-location-test-resources.c)
add_executable (db-info-test db-info-test.c)
add_executable (geo-test geo-test.c)

target_link_libraries (data-writer-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${ADDON_LIBRARIES} hyscancore)
target_link_libraries (acoustic-data-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${ADDON_LIBRARIES} hyscancore)
target_link_libraries (forward-look-data-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${ADDON_LIBRARIES} hyscancore)
target_link_libraries (db-info-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${ADDON_LIBRARIES} hyscancore)
target_link_libraries (location-test ${GLIB2_LIBRARIES} ${ADDON_LIBRARIES} hyscancore)
target_link_libraries (geo-test ${GLIB2_LIBRARIES} ${ADDON_LIBRARIES} hyscancore)

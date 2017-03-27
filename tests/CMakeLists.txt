
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${MATH_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_CORE_LIBRARY})

add_executable (data-writer-test data-writer-test.c)
add_executable (acoustic-data-test acoustic-data-test.c)
add_executable (nmea-data-test nmea-data-test.c)
add_executable (forward-look-data-test forward-look-data-test.c hyscan-fl-gen.c)
add_executable (forward-look-player-test forward-look-player-test.c hyscan-fl-gen.c)
add_executable (geo-test geo-test.c)
add_executable (depthometer-test depthometer-test.c)
add_executable (depth-nmea-test depth-nmea-test.c)
add_executable (tile-color-test tile-color-test.c)
add_executable (waterfall-tile-test waterfall-tile-test.c)
add_executable (tile-queue-test tile-queue-test.c)
add_executable (projector-test projector-test.c)
add_executable (waterfall-mark-data-test waterfall-mark-data-test.c)
add_executable (mark-manager-test mark-manager-test.c)

target_link_libraries (data-writer-test ${TEST_LIBRARIES})
target_link_libraries (acoustic-data-test ${TEST_LIBRARIES})
target_link_libraries (nmea-data-test ${TEST_LIBRARIES})
target_link_libraries (forward-look-data-test ${TEST_LIBRARIES})
target_link_libraries (forward-look-player-test ${TEST_LIBRARIES})
target_link_libraries (geo-test ${TEST_LIBRARIES})
target_link_libraries (depthometer-test ${TEST_LIBRARIES})
target_link_libraries (depth-nmea-test ${TEST_LIBRARIES})
target_link_libraries (tile-color-test ${TEST_LIBRARIES})
target_link_libraries (waterfall-tile-test ${TEST_LIBRARIES})
target_link_libraries (tile-queue-test ${TEST_LIBRARIES})
target_link_libraries (projector-test ${TEST_LIBRARIES})
target_link_libraries (waterfall-mark-data-test ${TEST_LIBRARIES})
target_link_libraries (mark-manager-test ${TEST_LIBRARIES})

install (TARGETS data-writer-test
                 acoustic-data-test
                 nmea-data-test
                 forward-look-data-test
                 forward-look-player-test
                 geo-test
                 depthometer-test
                 depth-nmea-test
                 projector-test
                 waterfall-mark-data-test
                 mark-manager-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

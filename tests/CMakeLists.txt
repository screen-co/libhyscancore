set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${MATH_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_CORE_LIBRARY})

add_executable (data-writer-test data-writer-test.c)
add_executable (acoustic-data-test acoustic-data-test.c)
add_executable (nmea-data-test nmea-data-test.c)
add_executable (forward-look-data-test forward-look-data-test.c hyscan-fl-gen.c)
add_executable (forward-look-player-test forward-look-player-test.c hyscan-fl-gen.c)
add_executable (geo-test geo-test.c)
add_executable (nmea-parser-test nmea-parser-test.c)
add_executable (depthometer-test depthometer-test.c)
add_executable (control-test control-test.c hyscan-dummy-device.c)
add_executable (projector-test projector-test.c)
add_executable (tile-color-test tile-color-test.c)
add_executable (waterfall-tile-test waterfall-tile-test.c)
add_executable (tile-queue-test tile-queue-test.c)
add_executable (waterfall-mark-data-test waterfall-mark-data-test.c)
add_executable (mark-model-test mark-model-test.c)
add_executable (mloc-test mloc-test.c)
add_executable (view-log view-log.c)

target_link_libraries (data-writer-test ${TEST_LIBRARIES})
target_link_libraries (acoustic-data-test ${TEST_LIBRARIES})
target_link_libraries (nmea-data-test ${TEST_LIBRARIES})
target_link_libraries (forward-look-data-test ${TEST_LIBRARIES})
target_link_libraries (forward-look-player-test ${TEST_LIBRARIES})
target_link_libraries (geo-test ${TEST_LIBRARIES})
target_link_libraries (nmea-parser-test ${TEST_LIBRARIES})
target_link_libraries (depthometer-test ${TEST_LIBRARIES})
target_link_libraries (control-test ${TEST_LIBRARIES})
target_link_libraries (projector-test ${TEST_LIBRARIES})
target_link_libraries (tile-color-test ${TEST_LIBRARIES})
target_link_libraries (waterfall-tile-test ${TEST_LIBRARIES})
target_link_libraries (tile-queue-test ${TEST_LIBRARIES})
target_link_libraries (waterfall-mark-data-test ${TEST_LIBRARIES})
target_link_libraries (mark-model-test ${TEST_LIBRARIES})
target_link_libraries (mloc-test ${TEST_LIBRARIES})
target_link_libraries (view-log ${TEST_LIBRARIES})

install (TARGETS data-writer-test
                 acoustic-data-test
                 nmea-data-test
                 forward-look-data-test
                 forward-look-player-test
                 geo-test
                 nmea-parser-test
                 depthometer-test
                 control-test
                 projector-test
                 tile-color-test
                 waterfall-tile-test
                 tile-queue-test
                 waterfall-mark-data-test
                 mark-model-test
                 mloc-test
                 view-log
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

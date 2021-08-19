set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${MATH_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_CORE_LIBRARY})

add_executable (data-writer-test data-writer-test.c)
add_executable (acoustic-data-test acoustic-data-test.c)
add_executable (factory-test factory-test.c)
add_executable (nmea-data-test nmea-data-test.c)
add_executable (nmea-cat nmea-cat.c)
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
add_executable (mloc-test mloc-test.c)
add_executable (view-log view-log.c)
add_executable (task-queue-test task-queue-test.c)
add_executable (mercator-test mercator-test.c)
add_executable (cartesian-test cartesian-test.c)
add_executable (nmea-file-device-test nmea-file-device-test.c)
add_executable (nav-model-test nav-model-test.c)
add_executable (nmea-file2udp nmea-file2udp.c)
add_executable (planner-test planner-test.c)
add_executable (planner-export-test planner-export-test.c)
add_executable (data-player-test data-player-test.c)
add_executable (nav-smooth-test nav-smooth-test.c)
add_executable (value-grid-test value-grid-test.c)
if (UNIX)
  add_executable (nmea-joystick nmea-joystick.c)
endif()
add_executable (hsx-converter-test hsx-converter-test.c)
add_executable (profile-test profile-test.c)
add_executable (object-data-test object-data-test.c)
add_executable (object-data-planner-test object-data-planner-test.c)

target_link_libraries (data-writer-test ${TEST_LIBRARIES})
target_link_libraries (acoustic-data-test ${TEST_LIBRARIES})
target_link_libraries (factory-test ${TEST_LIBRARIES})
target_link_libraries (nmea-data-test ${TEST_LIBRARIES})
target_link_libraries (nmea-cat ${TEST_LIBRARIES})
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
target_link_libraries (mloc-test ${TEST_LIBRARIES})
target_link_libraries (view-log ${TEST_LIBRARIES})
target_link_libraries (task-queue-test ${TEST_LIBRARIES})
target_link_libraries (mercator-test ${TEST_LIBRARIES})
target_link_libraries (cartesian-test ${TEST_LIBRARIES})
target_link_libraries (nmea-file-device-test ${TEST_LIBRARIES})
target_link_libraries (nav-model-test ${TEST_LIBRARIES})
target_link_libraries (nmea-file2udp ${TEST_LIBRARIES})
target_link_libraries (planner-test ${TEST_LIBRARIES})
target_link_libraries (planner-export-test ${TEST_LIBRARIES})
target_link_libraries (data-player-test ${TEST_LIBRARIES})
target_link_libraries (nav-smooth-test ${TEST_LIBRARIES})
target_link_libraries (value-grid-test ${TEST_LIBRARIES})
if (UNIX)
  target_link_libraries (nmea-joystick ${TEST_LIBRARIES})
endif()
target_link_libraries (hsx-converter-test ${TEST_LIBRARIES} ${PROJ4_LIBRARIES})
target_link_libraries (profile-test ${TEST_LIBRARIES})
target_link_libraries (object-data-test ${TEST_LIBRARIES})
target_link_libraries (object-data-planner-test ${TEST_LIBRARIES})

file (REMOVE_RECURSE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/db")
file (MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/db")
add_test (NAME DataWriterTest COMMAND data-writer-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME AcousticDataTest COMMAND acoustic-data-test -c 1024 -l 10 file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME NMEADataTest COMMAND nmea-data-test -s 10000 file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME ForwardLookDataTest COMMAND forward-look-data-test -l 500 -n 100000 -c 1024 file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME ForwardLookPlayerTest COMMAND forward-look-player-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME GeoTest COMMAND geo-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME ControlTest COMMAND control-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME TaskQueueTest COMMAND task-queue-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME MercatorTest COMMAND mercator-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME CartesianTest COMMAND cartesian-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME NavModelTest COMMAND nav-model-test --default-data
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME PlannerTest COMMAND planner-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME PlannerExportTest COMMAND planner-export-test -d file://db planner.xml
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME DataPlayerTest COMMAND data-player-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME NavSmoothTest COMMAND nav-smooth-test
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME ProfileTest COMMAND profile-test -d .
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME ObjectDataTest COMMAND object-data-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME ObjectDataPlannerTest COMMAND object-data-planner-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

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
                 mloc-test
                 view-log
                 task-queue-test
                 mercator-test
                 cartesian-test
                 nmea-file-device-test
                 nav-model-test
                 nmea-file2udp
                 planner-test
                 data-player-test
                 profile-test
                 object-data-test
                 object-data-planner-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

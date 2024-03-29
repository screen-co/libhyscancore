
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
add_executable (control-test control-test.c hyscan-dummy-device.c)
add_executable (view-log view-log.c)
add_executable (task-queue-test task-queue-test.c)
add_executable (object-data-test object-data-test.c)
add_executable (object-data-planner-test object-data-planner-test.c)

target_link_libraries (data-writer-test ${TEST_LIBRARIES})
target_link_libraries (acoustic-data-test ${TEST_LIBRARIES})
target_link_libraries (nmea-data-test ${TEST_LIBRARIES})
target_link_libraries (forward-look-data-test ${TEST_LIBRARIES})
target_link_libraries (forward-look-player-test ${TEST_LIBRARIES})
target_link_libraries (geo-test ${TEST_LIBRARIES})
target_link_libraries (control-test ${TEST_LIBRARIES})
target_link_libraries (view-log ${TEST_LIBRARIES})
target_link_libraries (task-queue-test ${TEST_LIBRARIES})
target_link_libraries (object-data-test ${TEST_LIBRARIES})
target_link_libraries (object-data-planner-test ${TEST_LIBRARIES})

file (REMOVE_RECURSE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/db")
file (MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/db")
add_test (NAME DataWriterTest COMMAND data-writer-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME AcousticDataTest COMMAND acoustic-data-test -c 1024 file://db
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
                 control-test
                 view-log
                 task-queue-test
                 object-data-test
         COMPONENT test
         RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)


add_executable (data-writer-test data-writer-test.c)
add_executable (raw-data-test raw-data-test.c)
add_executable (location-test location-test.c ${CMAKE_BINARY_DIR}/resources/hyscan-location-test-resources.c)
add_executable (geo-test geo-test.c)

target_link_libraries (data-writer-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (raw-data-test ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (location-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (geo-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)

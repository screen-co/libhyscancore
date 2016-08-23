
add_executable (data-writer-test data-writer-test.c)
add_executable (data-channel-test data-channel-test.c)
add_executable (convolution-test convolution-test.c)
add_executable (location-test location-test.c ${CMAKE_BINARY_DIR}/resources/hyscan-location-test-resources.c)
add_executable (geo-test geo-test.c)

target_link_libraries (data-writer-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (data-channel-test ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (convolution-test ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (location-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (geo-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)

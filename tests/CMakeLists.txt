
add_executable (data-channel-test data-channel-test.c)
add_executable (convolution-test convolution-test.c)
add_executable (state-test state-test.c)
add_executable (location-test location-test.c)

target_link_libraries (data-channel-test ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (convolution-test ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (state-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)
target_link_libraries (location-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore)

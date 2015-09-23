
add_executable( hyscan-data-channel-test hyscan-data-channel-test.c )
add_executable( hyscan-convolution-test hyscan-convolution-test.c )
add_executable( hyscan-state-test hyscan-state-test.c )

target_link_libraries( hyscan-data-channel-test m ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore )
target_link_libraries( hyscan-convolution-test m ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore )
target_link_libraries( hyscan-state-test ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscancore )

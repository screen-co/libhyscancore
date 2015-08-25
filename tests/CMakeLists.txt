
add_executable( hyscan-data-channel-test hyscan-data-channel-test.c )
add_executable( hyscan-state-test hyscan-state-test.c )

target_link_libraries( hyscan-data-channel-test ${GLIB2_LIBRARIES} hyscandb hyscancore )
target_link_libraries( hyscan-state-test ${GLIB2_LIBRARIES} hyscandb hyscancore )

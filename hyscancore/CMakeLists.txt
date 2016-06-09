
add_library (hyscancore SHARED
             pffft.c
             hyscan-core-types.c
             hyscan-state.c
             hyscan-convolution.c
             hyscan-data-channel.c
             hyscan-data-channel-writer.c
             hyscan-location-tools-nmea.c
             hyscan-location-tools-sounder.c
             hyscan-location-tools-filters.c
             hyscan-location-tools-overseer.c
             hyscan-location-tools-getters.c
             hyscan-location.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-location-test-resources.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c
             )

target_link_libraries (hyscancore ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})


add_library (hyscancore SHARED
             hyscan-core-types.c
             hyscan-data-writer.c
             hyscan-data-channel.c
             hyscan-geo.c
             hyscan-location-tools-nmea.c
             hyscan-location-tools-sounder.c
             hyscan-location-tools-filters.c
             hyscan-location-tools-assemblers.c
             hyscan-location-tools-overseer.c
             hyscan-location-tools-getters.c
             hyscan-location.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c)

target_link_libraries (hyscancore ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

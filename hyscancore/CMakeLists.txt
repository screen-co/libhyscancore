
add_library (hyscancore SHARED
             pffft.c
             hyscan-core-types.c
             hyscan-state.c
             hyscan-convolution.c
             hyscan-data-channel.c
             hyscan-data-channel-writer.c
             hyscan-seabed.c
             hyscan-seabed-echosounder.c
             hyscan-seabed-sonar.c
             ${CMAKE_BINARY_DIR}/resources/hyscan-core-resources.c)

target_link_libraries (hyscancore ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

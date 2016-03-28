
add_library (hyscancore SHARED
             pffft.c
             hyscan-state.c
             hyscan-convolution.c
             hyscan-data-channel.c
             hyscan-seabed.c
             hyscan-seabed-echosounder.c
             hyscan-seabed-sonar.c)

target_link_libraries (hyscancore ${ADDON_LIBRARIES} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

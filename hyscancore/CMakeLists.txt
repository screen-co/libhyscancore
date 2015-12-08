
add_library (hyscancore SHARED
             pffft.c
             hyscan-state.c
             hyscan-convolution.c
             hyscan-data-channel.c)

target_link_libraries (hyscancore m ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

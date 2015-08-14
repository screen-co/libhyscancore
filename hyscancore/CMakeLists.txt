
add_library( hyscancore SHARED
             pffft.c
             hyscan-core-types.c
             hyscan-data-channel.c )

target_link_libraries( hyscancore m ${GLIB2_LIBRARIES} ${HYSCAN_DB_LIBRARIES} )

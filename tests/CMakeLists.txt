
add_executable( data-channel data-channel.c )

target_link_libraries( data-channel ${GLIB2_LIBRARIES} hyscandb hyscancore )

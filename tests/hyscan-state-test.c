
#include <hyscan-state.h>
#include <hyscan-cached.h>

static void db_changed( HyScanState *state, HyScanDB *db, gpointer data )
{

  g_message( "db changed" );

}

static void cache_changed( HyScanState *state, HyScanCache *cache, gpointer data )
{

  g_message( "cache changed" );

}

static void project_changed( HyScanState *state, const gchar *project_name, gpointer data )
{

  g_message( "project changed: %s", project_name );

}

static void preset_changed( HyScanState *state, const gchar *preset_name, gpointer data )
{

  g_message( "preset changed: %s", preset_name );

}

static void track_changed( HyScanState *state, const gchar *track_name, gpointer data )
{

  g_message( "track changed: %s", track_name );

}

static void profile_changed( HyScanState *state, const gchar *profile_name, gpointer data )
{

  g_message( "profile changed: %s", profile_name );

}

int main( int argc, char **argv )
{

  HyScanCache *cache = HYSCAN_CACHE( hyscan_cached_new( 32 ) );
  HyScanState *state = hyscan_state_new();

  g_signal_connect( state, "db-changed", G_CALLBACK( db_changed ), NULL );
  g_signal_connect( state, "cache-changed", G_CALLBACK( cache_changed ), NULL );
  g_signal_connect( state, "project-changed", G_CALLBACK( project_changed ), NULL );
  g_signal_connect( state, "preset-changed", G_CALLBACK( preset_changed ), NULL );
  g_signal_connect( state, "track-changed", G_CALLBACK( track_changed ), NULL );
  g_signal_connect( state, "profile-changed", G_CALLBACK( profile_changed ), NULL );

  hyscan_state_set_db( state, NULL );
  hyscan_state_set_cache( state, cache );
  hyscan_state_set_project_name( state, "Project 1" );
  hyscan_state_set_preset_name( state, "default" );
  hyscan_state_set_track_name( state, "Track 1" );
  hyscan_state_set_profile_name( state, "Surveying" );

  g_object_unref( state );
  g_object_unref( cache );

  return 0;

}

#include <hyscan-data-channel.h>
#include <hyscan-db-file.h>
#include <string.h>
#include <math.h>

int main( int argc, char **argv )
{

  // Параметры программы.

  gchar *db_path = NULL;          // Путь к каталогу с базой данных.
  gdouble frequency = 0.0;        // Частота сигнала.
  gdouble duration = 0.0;         // Длительность сигнала.
  gdouble discretization = 0.0;   // Частота дискретизации.

  HyScanDB *db;
  HyScanDataChannel *dchannel1;
  HyScanDataChannel *dchannel2;

  gint32 project_id;
  gint32 track_id;
  gint32 signal_index;

  // Включим измерение времени.
  g_setenv( "TIME_MEASURE", "YES", TRUE );
  g_setenv( "G_MESSAGES_DEBUG", "all", TRUE );

  { // Разбор командной строки.

  gchar **args;
  GError          *error = NULL;
  GOptionContext  *context;
  GOptionEntry     entries[] =
  {
    { "discretization", 'd', 0, G_OPTION_ARG_DOUBLE, &discretization, "Signal discretization, Hz", NULL },
    { "frequency", 'f', 0, G_OPTION_ARG_DOUBLE, &frequency, "Signal frequency, Hz", NULL },
    { "duration", 't', 0, G_OPTION_ARG_DOUBLE, &duration, "Signal duration, s", NULL },
    { NULL }
  };

  #ifdef G_OS_WIN32
    args = g_win32_get_command_line();
  #else
    args = g_strdupv( argv );
  #endif

  context = g_option_context_new( "<db-path>" );
  g_option_context_set_help_enabled( context, TRUE );
  g_option_context_add_main_entries( context, entries, NULL );
  g_option_context_set_ignore_unknown_options( context, FALSE );
  if( !g_option_context_parse_strv( context, &args, &error ) )
    { g_message( error->message ); return -1; }

  if( g_strv_length( args ) != 2 || discretization < 1.0 || frequency < 1.0 || duration < 1e-7 )
    { g_print( "%s", g_option_context_get_help( context, FALSE, NULL ) ); return 0; }

  g_option_context_free( context );

  db_path = g_strdup( args[1] );
  g_strfreev( args );

  }


  { // Проверяем, что указанный каталог базы данных пустой.

  GDir *dir;

  // Создаём каталог, если его еще нет.
  if( g_mkdir_with_parents( db_path, 0777 ) != 0 ) g_error( "can't create directory '%s'", db_path );

  dir = g_dir_open( db_path, 0, NULL );
  if( dir == NULL ) g_error( "can't open directory '%s'", db_path );
  if( g_dir_read_name( dir ) != NULL ) g_error( "db directory '%s' must be empty", db_path );
  g_dir_close( dir );

  }


  // Открываем базу данных.
  db = g_object_new( G_TYPE_HYSCAN_DB_FILE, "path", db_path, NULL );

  // Создаём проект.
  project_id = hyscan_db_create_project( db, "project", NULL );
  if( project_id < 0 ) g_error( "can't create project" );

  // Создаём галс.
  track_id = hyscan_db_create_track( db, project_id, "track" );
  if( track_id < 0 ) g_error( "can't create track" );

  // Создаём образец сигнала для свёртки и записываем его.
  {

  guint32 signal_size = discretization * duration;
  HyScanComplexFloat *signal = g_malloc( signal_size * sizeof( HyScanComplexFloat ) );

  gint32 signal_id;
  gint i;

  g_message( "signal size = %d", signal_size );

  // Образец сигнала/
  for( i = 0; i < signal_size; i++ )
    {
    gdouble time = ( 1.0 / discretization ) * i;
    gdouble phase = 2.0 * G_PI * frequency * time;
    signal[i].re = cos( phase );
    signal[i].im = sin( phase );
    }

  // Открываем канал данных и записываем образец сигнала.
  signal_id = hyscan_db_create_channel( db, track_id, "signals" );
  if( signal_id < 0 ) g_error( "can't create signals" );

  if( !hyscan_db_add_channel_data( db, signal_id, 1, signal, signal_size * sizeof( HyScanComplexFloat ), &signal_index ) )
    g_error( "can't write signal image" );

  hyscan_db_close_channel( db, signal_id );

  g_free( signal );

  }


  // Создаём канал данных.
  {

  // Параметры канала данных.
  HyScanDataChannelInfo channel_info;

  channel_info.discretization_type = HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT;
  channel_info.discretization_frequency = discretization;

  channel_info.signal_type = HYSCAN_SIGNAL_TYPE_TONE;
  channel_info.signal_frequency = frequency;
  channel_info.signal_spectrum = frequency / 10.0;
  channel_info.signal_duration = duration;

  channel_info.antenna_x_offset = 0.0;
  channel_info.antenna_y_offset = 0.0;
  channel_info.antenna_z_offset = 0.0;

  channel_info.antenna_roll_offset = 0.0;
  channel_info.antenna_pitch_offset = 0.0;
  channel_info.antenna_yaw_offset = 0.0;

  channel_info.antenna_base_offset = 0.0;
  channel_info.antenna_hpattern = 0.0;
  channel_info.antenna_vpattern = 0.0;

  // Создаём каналы данных.
  dchannel1 = hyscan_data_channel_create( db, "project", "track", "data1", &channel_info, signal_index );
  if( dchannel1 == NULL ) g_error( "can't create 'data1' channel" );
  dchannel2 = hyscan_data_channel_create( db, "project", "track", "data2", &channel_info, signal_index );
  if( dchannel2 == NULL ) g_error( "can't create 'data2' channel" );

  }


  // Создаём тестовые данные для проверки свёртки.
  // Используется тональный сигнал. Строка размером 4 * signal_size  - '__S_'.
  {

  guint32 signal_size = discretization * duration;
  gint16 *data = g_malloc( 2 * 4 * signal_size * sizeof( gint16 ) );
  gint i;

  g_message( "data size = %d", 4 * signal_size );

  // Данные для первого канала.
  memset( data, 0, 2 * 4 * signal_size * sizeof( gint16 ) );
  for( i = 2 * signal_size; i < 3 * signal_size; i++ )
    {
    gdouble time = ( 1.0 / discretization ) * i;
    gdouble phase = 2.0 * G_PI * frequency * time;
    data[2 * i] = 32767.0 * cos( phase );
    data[2 * i + 1] = 32767.0 * sin( phase );
    }
  if( !hyscan_data_channel_add_data( dchannel1, 1, data, 2 * 4 * signal_size * sizeof( gint16 ), NULL ) )
    g_error( "can't write data line 1" );

  // Данные для второго канала - сдвинуты на Pi/2.
  memset( data, 0, 2 * 4 * signal_size * sizeof( gint16 ) );
  for( i = 2 * signal_size; i < 3 * signal_size; i++ )
    {
    gdouble time = ( 1.0 / discretization ) * i;
    gdouble phase = 2.0 * G_PI * frequency * time + G_PI / 2.0;
    data[2 * i] = 32767.0 * cos( phase );
    data[2 * i + 1] = 32767.0 * sin( phase );
    }
  if( !hyscan_data_channel_add_data( dchannel2, 1, data, 2 * 4 * signal_size * sizeof( gint16 ), NULL ) )
    g_error( "can't write data line 1" );

  g_free( data );

  }


  // Для тонального сигнала проверяем, что его свёртка совпадает с треугольником,
  // начинающимся с signal_size, пиком на 2 * signal_size и спадающим до 3 * signal_size.
  {

  gint32 signal_size = discretization * duration;
  gint32 buffer_size = 22 * signal_size;
  gint32 readings;

  gfloat *amp1 = g_malloc( 4 * signal_size * sizeof( gfloat ) );
  gfloat *amp2 = g_malloc( 4 * signal_size * sizeof( gfloat ) );
  gfloat *phase = g_malloc( 4 * signal_size * sizeof( gfloat ) );
  gfloat delta;

  gint i, j;

  // Аналитический вид амплитуды свёртки.
  memset( amp1, 0, 4 * signal_size * sizeof( gfloat ) );
  for( i = signal_size, j = 0; j < signal_size; i++, j++ )
    amp1[i] = (gfloat)j / signal_size;
  for( i = 2 * signal_size, j = 0; j < signal_size; i++, j++ )
    amp1[i] = 1.0 - (gfloat)j / signal_size;

  // Проверяем вид свёртки в первом канале.
  readings = buffer_size;
  hyscan_data_channel_get_amplitude_values( dchannel1, TRUE, 0, amp2, &readings, NULL );
  delta = 0.0;
  for( i = 0; i < 4 * signal_size; i++ )
    delta += fabs( amp1[i] - amp2[i] );
  g_message( "data1 mean amplitude error = %f", delta / ( 4.0 * signal_size ) );

  // Проверяем вид свёртки в втором канале.
  readings = buffer_size;
  hyscan_data_channel_get_amplitude_values( dchannel2, TRUE, 0, amp2, &readings, NULL );
  delta = 0.0;
  for( i = 0; i < 4 * signal_size; i++ )
    delta += fabs( amp1[i] - amp2[i] );
  g_message( "data2 mean amplitude error = %f", delta / ( 4.0 * signal_size ) );

  // Проверяем разницу фаз между двумя каналами.
  readings = buffer_size;
  hyscan_data_channel_get_phase_values( dchannel1, dchannel2, TRUE, 0, phase, &readings, NULL );
  delta = 0.0;
  for( i = signal_size; i < 3 * signal_size; i++ )
    delta += fabs( phase[i] + G_PI / 2.0 );
  g_message( "phase mean amplitude error = %f", delta / ( 2.0 * signal_size ) );

  // Выполним 1000 вызовов функции свёртки, для вычисления скорости работы.
  g_message( "data1 speed test" );
  for( i = 0; i < 1000; i++ )
    hyscan_data_channel_get_amplitude_values( dchannel1, TRUE, 0, amp2, &readings, NULL );
  g_message( "data2 speed test" );
  for( i = 0; i < 1000; i++ )
    hyscan_data_channel_get_amplitude_values( dchannel2, TRUE, 0, amp2, &readings, NULL );

  }

  // Закрываем каналы данных.
  g_object_unref( dchannel1 );
  g_object_unref( dchannel2 );

  // Закрываем галс и проект.
  hyscan_db_close_project( db, project_id );
  hyscan_db_close_track( db, track_id );

  // Закрываем базу данных.
  g_object_unref( db );

  return 0;

}

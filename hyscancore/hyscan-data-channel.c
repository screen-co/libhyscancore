/*!
 * \file hyscan-data-channel.c
 *
 * \brief Исходный файл класса первичной обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-data-channel.h"
#include "hyscan-convolution.h"

#include <math.h>
#include <string.h>

#ifdef HYSCAN_CORE_USE_OPENMP
#include <omp.h>
#endif


#define DATA_CHANNEL_API       20150700             // Версия API канала гидролокационных данных.
#define SIGNALS_CHANNEL_NAME   "signals"            // Название канала данных с образцами излучавшихся сигналов.


enum { PROP_O, PROP_DB, PROP_CACHE, PROP_CACHE_PREFIX };

enum { DATA_TYPE_AMPLITUDE, DATA_TYPE_QUADRATURE };


typedef struct HyScanDataChannelPriv {              // Внутренние данные объекта.

  HyScanDB                  *db;                    // Интерфейс базы данных.
  gchar                     *uri;                   // URI базы данных.

  HyScanCache               *cache;                 // Интерфейс системы кэширования.
  gchar                     *cache_prefix;          // Префикс ключа кэширования.
  gchar                     *cache_key;             // Ключ кэширования.
  gint                       cache_key_length;      // Максимальная длина ключа.

  gchar                     *project_name;          // Название проекта.
  gchar                     *track_name;            // Название галса.
  gchar                     *channel_name;          // Название канала данных.
  gint32                     channel_id;            // Идентификатор открытого канала данных.

  HyScanDataChannelInfo      info;                  // Информация о канале данных.

  gint64                     index_time;            // Метка времени обрабатываемых данных.
  HyScanComplexFloat        *data_buffer;           // Буфер для обработки данных.
  gpointer                   raw_buffer;            // Буфер для чтения "сырых" данных канала.
  gint32                     raw_buffer_size;       // Размер буфера в байтах.

  HyScanConvolution         *convolution;           // Свёртка данных.
  gboolean                   convolve;              // Выполнять или нет свёртку.

  GMutex                     lock;                  // Блокировка многопоточного доступа.

  gboolean                   readonly;              // Объект в режиме только для чтения.

} HyScanDataChannelPriv;


#define HYSCAN_DATA_CHANNEL_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannelPriv ) )


static void hyscan_data_channel_set_property( HyScanDataChannel *dchannel, guint prop_id, const GValue *value, GParamSpec *pspec );
static GObject* hyscan_data_channel_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties );
static void hyscan_data_channel_object_finalize( HyScanDataChannel *dchannel );

static void hyscan_data_channel_create_cache_key( HyScanDataChannelPriv *priv, gint data_type, gint32 index );
static void hyscan_data_channel_buffer_realloc( HyScanDataChannelPriv *priv, gint32 size );
static gint32 hyscan_data_channel_read_data( HyScanDataChannelPriv *priv, gint32 index );
static gboolean hyscan_data_channel_check_cache( HyScanDataChannelPriv *priv, gint32 index, gint data_type, gpointer buffer, gint32 *buffer_size, gint64 *time );

static gboolean hyscan_data_channel_create_int( HyScanDataChannelPriv *priv, const gchar *project_name, const gchar *track_name, const gchar *channel_name, HyScanDataChannelInfo *info, gint32 signal_index );
static gboolean hyscan_data_channel_open_int( HyScanDataChannelPriv *priv, const gchar *project_name, const gchar *track_name, const gchar *channel_name );
static void hyscan_data_channel_close_int( HyScanDataChannelPriv *priv );


G_DEFINE_TYPE( HyScanDataChannel, hyscan_data_channel, G_TYPE_OBJECT );


static void hyscan_data_channel_init( HyScanDataChannel *dchannel ){ ; }


static void hyscan_data_channel_class_init( HyScanDataChannelClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->set_property = hyscan_data_channel_set_property;

  this_class->constructor = hyscan_data_channel_object_constructor;
  this_class->finalize = hyscan_data_channel_object_finalize;

  g_object_class_install_property( this_class, PROP_DB,
    g_param_spec_object( "db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_CACHE,
    g_param_spec_object( "cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_CACHE_PREFIX,
    g_param_spec_string( "cache-prefix", "CachePrefix", "Cache key prefix", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_type_class_add_private( klass, sizeof( HyScanDataChannelPriv ) );

}


static void hyscan_data_channel_set_property( HyScanDataChannel *dchannel, guint prop_id, const GValue *value, GParamSpec *pspec )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  switch ( prop_id )
    {

    case PROP_DB:
      priv->db = g_value_get_object( value );
      if( priv->db ) g_object_ref( priv->db );
      else G_OBJECT_WARN_INVALID_PROPERTY_ID( dchannel, prop_id, pspec );
      break;

    case PROP_CACHE:
      priv->cache = g_value_get_object( value );
      if( priv->cache ) g_object_ref( priv->cache );
      break;

    case PROP_CACHE_PREFIX:
      priv->cache_prefix = g_value_dup_string( value );
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( dchannel, prop_id, pspec );
      break;

    }

}


static GObject* hyscan_data_channel_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties )
{

  GObject *dchannel = G_OBJECT_CLASS( hyscan_data_channel_parent_class )->constructor( g_type, n_construct_properties, construct_properties );
  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  // Начальные значения.
  priv->channel_id = -1;
  priv->readonly = TRUE;

  // Блокировка.
  g_mutex_init( &priv->lock );

  // Свёртка данных.
  priv->convolution = hyscan_convolution_new();

  // URI базы данных.
  priv->uri = hyscan_db_get_uri( priv->db );

  return dchannel;

}


static void hyscan_data_channel_object_finalize( HyScanDataChannel *dchannel )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  hyscan_data_channel_close( dchannel );

  g_free( priv->uri );
  g_free( priv->raw_buffer );
  g_free( priv->data_buffer );

  if( priv->db ) g_object_unref( priv->db );
  if( priv->cache ) g_object_unref( priv->cache );

  g_object_unref( priv->convolution );
  g_mutex_clear( &priv->lock );

  G_OBJECT_CLASS( hyscan_data_channel_parent_class )->finalize( G_OBJECT( dchannel ) );

}


// Функция создаёт ключ кэширования данных.
static void hyscan_data_channel_create_cache_key( HyScanDataChannelPriv *priv, gint data_type, gint32 index )
{

  gchar *data_type_str;

  if( data_type == DATA_TYPE_AMPLITUDE ) data_type_str = "A";
  else data_type_str = "Q";

  // Ключ кэширования.
  if( priv->cache_prefix )
    g_snprintf( priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%s.%s.%s.%d", priv->uri, priv->cache_prefix, priv->project_name, priv->track_name, priv->channel_name, priv->convolve ? "CV" : "NC", data_type_str, index );
  else
    g_snprintf( priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%s.%s.%d", priv->uri, priv->project_name, priv->track_name, priv->channel_name, priv->convolve ? "CV" : "NC", data_type_str, index );

}


// Функция проверяет размер буферов и увеличивает его при необходимости.
static void hyscan_data_channel_buffer_realloc( HyScanDataChannelPriv *priv, gint32 size )
{

  if( priv->raw_buffer_size > size ) return;

  priv->raw_buffer_size = size + 32;
  priv->raw_buffer = g_realloc( priv->raw_buffer, priv->raw_buffer_size );
  priv->data_buffer = g_realloc( priv->data_buffer,
    priv->raw_buffer_size / hyscan_get_data_point_size( priv->info.discretization_type ) * sizeof( HyScanComplexFloat ) );

}


// Функция считывает "сырые" акустические данные для указанного индекса и импортирует их в буфер данных.
static gint32 hyscan_data_channel_read_data( HyScanDataChannelPriv *priv, gint32 index )
{

  gint32 io_size;
  gint32 n_points;
  gint32 point_size = hyscan_get_data_point_size( priv->info.discretization_type );

  // Проверка состояния объекта.
  if( priv->channel_id < 0 ) return -1;

  // Считываем данные канала.
  io_size = priv->raw_buffer_size;
  if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, priv->raw_buffer, &io_size, &priv->index_time ) ) return -1;

  // Если размер считанных данных равен размеру буфера, запрашиваем реальный размер,
  // увеличиваем размер буфера и считываем данные еще раз.
  if( priv->raw_buffer_size == 0 || priv->raw_buffer_size == io_size )
    {
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, NULL, &io_size, NULL ) ) return -1;
    hyscan_data_channel_buffer_realloc( priv, io_size );
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, priv->raw_buffer, &io_size, &priv->index_time ) ) return -1;
    }

  // Размер считанных данных должен быть кратен размеру точки.
  if( io_size % point_size ) return -1;
  n_points = io_size / point_size;

  // Импортируем данные в буфер обработки.
  hyscan_import_data( priv->info.discretization_type, priv->raw_buffer, io_size, priv->data_buffer, &n_points );

  // Выполняем свёртку.
  if( priv->convolve ) hyscan_convolution_convolve( priv->convolution, priv->data_buffer, n_points );

  return n_points;

}


// Функция проверяет кэш на наличие данных указанного типа и если такие есть считывает их.
static gboolean hyscan_data_channel_check_cache( HyScanDataChannelPriv *priv, gint data_type, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time )
{

  gint64 cached_time;
  gint32 time_size;
  gint32 io_size;

  gint data_type_size;

  if( !priv->cache || !buffer ) return FALSE;

  if( data_type == DATA_TYPE_AMPLITUDE ) data_type_size = sizeof( gfloat );
  else data_type_size = sizeof( HyScanComplexFloat );

  // Ключ кэширования.
  hyscan_data_channel_create_cache_key( priv, data_type, index );

  // Ищем данные в кэше.
  time_size = sizeof( cached_time );
  io_size = *buffer_size * sizeof( gfloat );
  if( !hyscan_cache_get2( priv->cache, priv->cache_key, NULL, &cached_time, &time_size, buffer, &io_size ) ) return FALSE;
  if( time_size != sizeof( cached_time ) || io_size < data_type_size || io_size % data_type_size ) return FALSE;

  if( time ) *time = cached_time;
  *buffer_size = io_size / data_type_size;
  return TRUE;

}


// Функция создаёт новый канал хранения акустических данных и открывает его для первичной обработки.
static gboolean hyscan_data_channel_create_int( HyScanDataChannelPriv *priv, const gchar *project_name, const gchar *track_name, const gchar *channel_name, HyScanDataChannelInfo *info, gint32 signal_index )
{

  gboolean status = FALSE;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 channel_id = -1;
  gint32 param_id = -1;

  // Проверяем, что передан указатель на базу данных.
  if( !priv->db ) goto exit;

  // Открываем проект.
  project_id = hyscan_db_open_project( priv->db, project_name );
  if( project_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't open project '%s'", project_name );
    goto exit;
    }

  // Открываем галс.
  track_id = hyscan_db_open_track( priv->db, project_id, track_name );
  if( track_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't open track '%s.%s'", project_name, track_name );
    goto exit;
    }

  // Создаём канал данных.
  channel_id = hyscan_db_create_channel( priv->db, track_id, channel_name );
  if( track_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't create channel '%s.%s.%s'", project_name, track_name, channel_name );
    goto exit;
    }

  // Параметры канала данных.
  param_id = hyscan_db_open_channel_param( priv->db, channel_id );
  if( param_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't set channel '%s.%s.%s' parameters", project_name, track_name, channel_name );
    goto exit;
    }

  hyscan_db_set_string_param( priv->db, param_id, "channel.version", "20150700" );

  hyscan_db_set_string_param( priv->db, param_id, "discretization.type", hyscan_get_data_type_name( info->discretization_type ) );
  hyscan_db_set_double_param( priv->db, param_id, "discretization.frequency", info->discretization_frequency );

  hyscan_db_set_string_param( priv->db, param_id, "signal.type", hyscan_get_signal_type_name( info->signal_type ) );
  hyscan_db_set_double_param( priv->db, param_id, "signal.frequency", info->signal_frequency );
  hyscan_db_set_double_param( priv->db, param_id, "signal.spectrum", info->signal_spectrum );
  hyscan_db_set_double_param( priv->db, param_id, "signal.duration", info->signal_duration );
  if( signal_index >= 0 ) hyscan_db_set_integer_param( priv->db, param_id, "signal.index", signal_index );

  hyscan_db_set_double_param( priv->db, param_id, "antenna.x-offset", info->antenna_x_offset );
  hyscan_db_set_double_param( priv->db, param_id, "antenna.y-offset", info->antenna_y_offset );
  hyscan_db_set_double_param( priv->db, param_id, "antenna.z-offset", info->antenna_z_offset );

  hyscan_db_set_double_param( priv->db, param_id, "antenna.roll-offset", info->antenna_roll_offset );
  hyscan_db_set_double_param( priv->db, param_id, "antenna.pitch-offset", info->antenna_pitch_offset );
  hyscan_db_set_double_param( priv->db, param_id, "antenna.yaw-offset", info->antenna_yaw_offset );

  hyscan_db_set_double_param( priv->db, param_id, "antenna.base-offset", info->antenna_base_offset );
  hyscan_db_set_double_param( priv->db, param_id, "antenna.hpattern", info->antenna_hpattern );
  hyscan_db_set_double_param( priv->db, param_id, "antenna.vpattern", info->antenna_vpattern );

  status = TRUE;

  exit:

  if( status ) status = hyscan_data_channel_open_int( priv, project_name, track_name, channel_name );
  if( param_id > 0 ) hyscan_db_close_param( priv->db, param_id );
  if( channel_id > 0 ) hyscan_db_close_channel( priv->db, channel_id );
  if( track_id > 0 ) hyscan_db_close_track( priv->db, track_id );
  if( project_id > 0 ) hyscan_db_close_project( priv->db, project_id );

  return status;

}


// Функция открывает существующий канал хранения акустических данных для первичной обработки.
static gboolean hyscan_data_channel_open_int( HyScanDataChannelPriv *priv, const gchar *project_name, const gchar *track_name, const gchar *channel_name )
{

  gboolean status = FALSE;

  gchar *type;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;
  gint32 signals_id = -1;

  // Проверяем, что передан указатель на базу данных.
  if( !priv->db ) goto exit;

  // Проверяем, что заданы имя проекта, галса и канала данных.
  if( !project_name || !track_name || !channel_name )
    {
    if( !project_name ) g_critical( "hyscan_data_channel_open: unknown project name" );
    if( !track_name ) g_critical( "hyscan_data_channel_open: unknown track name" );
    if( !channel_name ) g_critical( "hyscan_data_channel_open: unknown channel name" );
    goto exit;
    }

  // Сохраняем имена проекта, галса и канала данных.
  priv->project_name = g_strdup( project_name );
  priv->track_name = g_strdup( track_name );
  priv->channel_name = g_strdup( channel_name );

  // Открываем проект.
  project_id = hyscan_db_open_project( priv->db, priv->project_name );
  if( project_id < 0 )
    {
    g_critical( "hyscan_data_channel_open: can't open project '%s'", priv->project_name );
    goto exit;
    }

  // Открываем галс.
  track_id = hyscan_db_open_track( priv->db, project_id, priv->track_name );
  if( track_id < 0 )
    {
    g_critical( "hyscan_data_channel_open: can't open track '%s.%s'", priv->project_name, priv->track_name );
    goto exit;
    }

  // Открываем канал данных.
  priv->channel_id = hyscan_db_open_channel( priv->db, track_id, priv->channel_name );
  if( priv->channel_id < 0 )
    {
    g_critical( "hyscan_data_channel_open: can't open channel '%s.%s.%s'", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Открываем группу параметров канала данных.
  param_id = hyscan_db_open_channel_param( priv->db, priv->channel_id );
  if( param_id < 0 )
    {
    g_critical( "hyscan_data_channel_open: can't open channel '%s.%s.%s' parameters", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Проверяем версию API канала данных.
  if( (guint32)( hyscan_db_get_integer_param( priv->db, param_id, "channel.version" ) / 100 ) != (guint32)( DATA_CHANNEL_API / 100 ) )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unsupported api version (%d)", priv->project_name, priv->track_name, priv->channel_name, (guint32)hyscan_db_get_integer_param( priv->db, param_id, "channel.version" ) );
    goto exit;
    }

  // Тип дискретизации данных в канале.
  type = hyscan_db_get_string_param( priv->db, param_id, "discretization.type" );
  priv->info.discretization_type = hyscan_get_data_type_by_name( type );
  g_free( type );
  if( priv->info.discretization_type == HYSCAN_DATA_TYPE_UNKNOWN )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unknown discretization type", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Частота дискретизации данных.
  priv->info.discretization_frequency = hyscan_db_get_double_param( priv->db, param_id, "discretization.frequency" );
  if( priv->info.discretization_frequency < 1.0 )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unsupported discretization frequency %.3fHz", priv->project_name, priv->track_name, priv->channel_name, priv->info.discretization_frequency );
    goto exit;
    }

  // Тип рабочего сигнала.
  type = hyscan_db_get_string_param( priv->db, param_id, "signal.type" );
  priv->info.signal_type = hyscan_get_signal_type_by_name( type );
  g_free( type );
  if( priv->info.signal_type == HYSCAN_SIGNAL_TYPE_UNKNOWN )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unknown signal type", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Рабочая (центральная) частота сигнала
  priv->info.signal_frequency = hyscan_db_get_double_param( priv->db, param_id, "signal.frequency" );
  if( priv->info.signal_frequency < 1.0 )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unsupported signal frequency %.3fHz", priv->project_name, priv->track_name, priv->channel_name, priv->info.signal_frequency );
    goto exit;
    }

  // Ширина спектра сигнала.
  priv->info.signal_spectrum = hyscan_db_get_double_param( priv->db, param_id, "signal.spectrum" );
  if( priv->info.signal_spectrum < 1.0 )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unsupported signal spectrum %.3fHz", priv->project_name, priv->track_name, priv->channel_name, priv->info.signal_spectrum );
    goto exit;
    }

  // Длительность сигнала
  priv->info.signal_duration = hyscan_db_get_double_param( priv->db, param_id, "signal.duration" );
  if( priv->info.signal_duration < 1e-7 )
    {
    g_critical( "hyscan_data_channel_open: '%s.%s.%s': unsupported signal duration %.3fms", priv->project_name, priv->track_name, priv->channel_name, 1e3 * priv->info.signal_duration );
    goto exit;
    }

  // Смещения местоположения антенны.
  priv->info.antenna_x_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.x-offset" );
  priv->info.antenna_y_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.y-offset" );
  priv->info.antenna_z_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.z-offset" );

  // Углы установки антенны.
  priv->info.antenna_roll_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.roll_offset" );
  priv->info.antenna_pitch_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.pitch_offset" );
  priv->info.antenna_yaw_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.yaw_offset" );

  // Характеристики антенны.
  priv->info.antenna_base_offset = hyscan_db_get_double_param( priv->db, param_id, "antenna.base-offset" );
  priv->info.antenna_hpattern = hyscan_db_get_double_param( priv->db, param_id, "antenna.hpattern" );
  priv->info.antenna_vpattern = hyscan_db_get_double_param( priv->db, param_id, "antenna.vpattern" );

  // Проверяем необходимость использования образца сигнала для свёртки.
  if( hyscan_db_has_param( priv->db, param_id, "signal.index" ) )
    {

    gint signal_image_index;

    HyScanComplexFloat *signal;
    gint32 n_signal_points;
    gint32 signal_size;

    // Образцы сигналов для свёртки.
    signals_id = hyscan_db_open_channel( priv->db, track_id, SIGNALS_CHANNEL_NAME );
    if( signals_id < 0 )
      {
      g_critical( "hyscan_data_channel_open: '%s.%s.%s': can't open signals", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Размер образца сигнала для свёртки.
    signal_image_index = hyscan_db_get_integer_param( priv->db, param_id, "signal.index" );
    if( !hyscan_db_get_channel_data( priv->db, signals_id, signal_image_index, NULL, &signal_size, NULL ) )
      {
      g_critical( "hyscan_data_channel_open: '%s.%s.%s': can't get signal image size", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Размер образца сигнала должен быть кратен размеру структуры HyScanComplexFloat.
    if( ( signal_size % sizeof( HyScanComplexFloat ) ) != 0 )
      {
      g_critical( "hyscan_data_channel_open: '%s.%s.%s': unsupported signal image size", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Считываем образец сигнала.
    signal = g_malloc( signal_size );
    if( !hyscan_db_get_channel_data( priv->db, signals_id, signal_image_index, signal, &signal_size, NULL ) )
      {
      g_free( signal );
      g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': can't get signal image", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Устанавливаем образец сигнала для свёртки.
    n_signal_points = signal_size / sizeof( HyScanComplexFloat );
    if( !hyscan_convolution_set_image( priv->convolution, signal, n_signal_points ) )
      {
      g_free( signal );
      g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': can't set signal image", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    priv->convolve = TRUE;

    g_free( signal );

    }

  // Ключ кэширования.
  if( priv->cache )
    {
    if( priv->cache_prefix )
      priv->cache_key = g_strdup_printf( "%s.%s.%s.%s.CV.A.0123456789", priv->cache_prefix, priv->project_name, priv->track_name, priv->channel_name );
    else
      priv->cache_key = g_strdup_printf( "%s.%s.%s.CV.A.0123456789", priv->project_name, priv->track_name, priv->channel_name );
    priv->cache_key_length = 2 * strlen( priv->cache_key );
    priv->cache_key = g_realloc( priv->cache_key, priv->cache_key_length );
    }

  priv->readonly = TRUE;
  status = TRUE;

  exit:

  if( !status ) hyscan_data_channel_close_int( priv );
  if( project_id > 0 ) hyscan_db_close_project( priv->db, project_id );
  if( track_id > 0 ) hyscan_db_close_track( priv->db, track_id );
  if( param_id > 0 ) hyscan_db_close_param( priv->db, param_id );
  if( signals_id > 0 ) hyscan_db_close_channel( priv->db, signals_id );

  return status;

}


// Функция закрывает текущий обрабатываемый канал данных.
static void hyscan_data_channel_close_int( HyScanDataChannelPriv *priv )
{

  // Закрываем канал данных.
  if( priv->channel_id > 0 ) hyscan_db_close_channel( priv->db, priv->channel_id );
  priv->channel_id = -1;

  // Обнуляем имена проекта, галса и канала данных.
  g_clear_pointer( &priv->project_name, g_free );
  g_clear_pointer( &priv->track_name, g_free );
  g_clear_pointer( &priv->channel_name, g_free );

  // Обнуляем буфер ключа кэширования.
  g_clear_pointer( &priv->cache_key, g_free );

  // Обнуляем информацию о канале данных.
  memset( &priv->info, 0, sizeof( HyScanDataChannelInfo ) );

}


// Функция создаёт новый объект первичной обработки акустических данных.
HyScanDataChannel *hyscan_data_channel_new( HyScanDB *db, HyScanCache *cache, const gchar *cache_prefix )
{

  return g_object_new( HYSCAN_TYPE_DATA_CHANNEL, "db", db, "cache", cache, "cache-prefix", cache_prefix, NULL );

}


// Функция создаёт новый канал хранения акустических данных и открывает его для первичной обработки.
gboolean hyscan_data_channel_create( HyScanDataChannel *dchannel, const gchar *project_name, const gchar *track_name, const gchar *channel_name, HyScanDataChannelInfo *info, gint32 signal_index )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  hyscan_data_channel_close( dchannel );

  g_mutex_lock( &priv->lock );
  status = hyscan_data_channel_create_int( priv, project_name, track_name, channel_name, info, signal_index );
  if( status ) priv->readonly = FALSE;
  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция открывает существующий канал хранения акустических данных для первичной обработки.
gboolean hyscan_data_channel_open( HyScanDataChannel *dchannel, const gchar *project_name, const gchar *track_name, const gchar *channel_name )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  hyscan_data_channel_close( dchannel );

  g_mutex_lock( &priv->lock );
  status = hyscan_data_channel_open_int( priv, project_name, track_name, channel_name );
  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция закрывает текущий обрабатываемый канал данных.
void hyscan_data_channel_close( HyScanDataChannel *dchannel )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  g_mutex_lock( &priv->lock );
  hyscan_data_channel_close_int( priv );
  g_mutex_unlock( &priv->lock );

}


// Функция возвращает параметры акустических данных.
HyScanDataChannelInfo *hyscan_data_channel_get_info( HyScanDataChannel *dchannel )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );
  HyScanDataChannelInfo *info = g_malloc( sizeof( HyScanDataChannelInfo ) );

  g_mutex_lock( &priv->lock );

  info->discretization_type = priv->info.discretization_type;
  info->discretization_frequency = priv->info.discretization_frequency;

  info->signal_type = priv->info.signal_type;
  info->signal_frequency = priv->info.signal_frequency;
  info->signal_spectrum = priv->info.signal_spectrum;
  info->signal_duration = priv->info.signal_duration;

  info->antenna_x_offset = priv->info.antenna_x_offset;
  info->antenna_y_offset = priv->info.antenna_y_offset;
  info->antenna_z_offset = priv->info.antenna_z_offset;

  info->antenna_roll_offset = priv->info.antenna_roll_offset;
  info->antenna_pitch_offset = priv->info.antenna_pitch_offset;
  info->antenna_yaw_offset = priv->info.antenna_yaw_offset;

  info->antenna_base_offset = priv->info.antenna_base_offset;
  info->antenna_hpattern = priv->info.antenna_hpattern;
  info->antenna_vpattern = priv->info.antenna_vpattern;

  g_mutex_unlock( &priv->lock );

  return info;

}


// Функция задаёт новый образец сигнала для свёртки.
gboolean hyscan_data_channel_set_signal_image( HyScanDataChannel *dchannel, HyScanComplexFloat *signal, gint32 n_signal_points )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  g_mutex_lock( &priv->lock );
  status = hyscan_convolution_set_image( priv->convolution, signal, n_signal_points );
  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция устанавливает необходимость выполнения свёртки акустических данных.
void hyscan_data_channel_set_convolve( HyScanDataChannel *dchannel, gboolean convolve )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  g_mutex_lock( &priv->lock );
  priv->convolve = convolve;
  g_mutex_unlock( &priv->lock );

}


// Функция возвращает диапазон значений индексов записанных данных.
gboolean hyscan_data_channel_get_range( HyScanDataChannel *dchannel, gint32 *first_index, gint32 *last_index )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  g_mutex_lock( &priv->lock );
  if( priv->channel_id > 0 )
    status = hyscan_db_get_channel_data_range( priv->db, priv->channel_id, first_index, last_index );
  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция возвращает число точек данных для указанного индекса.
gint32 hyscan_data_channel_get_values_count( HyScanDataChannel *dchannel, gint32 index )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gint32 dsize = -1;

  g_mutex_lock( &priv->lock );
  if( priv->channel_id > 0 )
    if( hyscan_db_get_channel_data( priv->db, priv->channel_id, index, NULL, &dsize, NULL ) )
      dsize /= hyscan_get_data_point_size( priv->info.discretization_type );
  g_mutex_unlock( &priv->lock );

  return dsize;

}


// Функция ищет индекс данных для указанного момента времени.
gboolean hyscan_data_channel_find_data( HyScanDataChannel *dchannel, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  g_mutex_lock( &priv->lock );
  if( priv->channel_id > 0 )
    status = hyscan_db_find_channel_data( priv->db, priv->channel_id, time, lindex, rindex, ltime, rtime );
  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция записывает новые данные в канал.
gboolean hyscan_data_channel_add_data( HyScanDataChannel *dchannel, gint64 time, gpointer data, gint32 size, gint32 *index )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;
  gfloat *buffer;
  gint32 n_points;
  gint32 i;

  g_mutex_lock( &priv->lock );

  // Проверяем размер буферов.
  if( priv->raw_buffer_size < size ) hyscan_data_channel_buffer_realloc( priv, size );

  // Импортируем данные в буфер обработки.
  n_points = size / hyscan_get_data_point_size( priv->info.discretization_type );
  hyscan_import_data( priv->info.discretization_type, data, size, priv->data_buffer, &n_points );
  buffer = (gpointer)priv->data_buffer;

  // Выполняем свёртку.
  if( priv->convolve ) hyscan_convolution_convolve( priv->convolution, priv->data_buffer, n_points );

  // Рассчитываем амплитуду.
  for( i = 0; i < n_points; i++ )
    {
    gfloat re = priv->data_buffer[i].re;
    gfloat im = priv->data_buffer[i].im;
    buffer[i] = sqrtf( re*re + im*im );
    }

  // Сохраняем данные в кэше.
//  if( priv->cache && buffer )
//    hyscan_cache_set2( priv->cache, priv->cache_key, NULL, &time, sizeof( time ), buffer, n_points * sizeof( gfloat ) );

  if( priv->channel_id > 0 && !priv->readonly )
    status = hyscan_db_add_channel_data( priv->db, priv->channel_id, time, data, size, index );

  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция возвращает необработанные акустические данные в формате их записи.
gboolean hyscan_data_channel_get_raw_values( HyScanDataChannel *dchannel, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  g_mutex_lock( &priv->lock );
  status = hyscan_db_get_channel_data( priv->db, priv->channel_id, index, buffer, buffer_size, time );
  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция возвращает значения амплитуды акустического сигнала.
gboolean hyscan_data_channel_get_amplitude_values( HyScanDataChannel *dchannel, gint32 index, gfloat *buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;
  gfloat *amplitude;
  gint32 n_points;
  gint32 i;

  g_mutex_lock( &priv->lock );

  // Проверяем наличие данных в кэше.
  if( hyscan_data_channel_check_cache( priv, DATA_TYPE_AMPLITUDE, index, buffer, buffer_size, time ) )
    { status = TRUE; goto exit; }

  // Считываем данные канала.
  n_points = hyscan_data_channel_read_data( priv, index );
  if( n_points <= 0 ) goto exit;

  // Возвращаем амплитуду.
  *buffer_size = MIN( *buffer_size, n_points );
  amplitude = (gpointer)priv->data_buffer;
  for( i = 0; i < n_points; i++ )
    {
    gfloat re = priv->data_buffer[i].re;
    gfloat im = priv->data_buffer[i].im;
    amplitude[i] = sqrtf( re*re + im*im );
    }
  memcpy( buffer, amplitude, *buffer_size * sizeof( gfloat ) );

  // Сохраняем данные в кэше.
  if( priv->cache && buffer )
    hyscan_cache_set2( priv->cache, priv->cache_key, NULL, &priv->index_time, sizeof( priv->index_time ), amplitude, n_points * sizeof( gfloat ) );

  if( time ) *time = priv->index_time;
  status = TRUE;

  exit:

  g_mutex_unlock( &priv->lock );

  return status;

}


// Функция возвращает квадратурные отсчёты акустического сигнала.
gboolean hyscan_data_channel_get_quadrature_values( HyScanDataChannel *dchannel, gint32 index, HyScanComplexFloat *buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;
  gint32 n_points;

  g_mutex_lock( &priv->lock );

  // Проверяем наличие данных в кэше.
  if( hyscan_data_channel_check_cache( priv, DATA_TYPE_QUADRATURE, index, buffer, buffer_size, time ) )
    { status = TRUE; goto exit; }

  // Считываем данные канала.
  n_points = hyscan_data_channel_read_data( priv, index );
  if( n_points <= 0 ) goto exit;

  // Возвращаем квадратурные отсчёты.
  *buffer_size = MIN( *buffer_size, n_points );
  memcpy( buffer, priv->data_buffer, *buffer_size * sizeof( HyScanComplexFloat ) );

  // Сохраняем данные в кэше.
  if( priv->cache && buffer )
    hyscan_cache_set2( priv->cache, priv->cache_key, NULL, &priv->index_time, sizeof( priv->index_time ), priv->data_buffer, n_points * sizeof( HyScanComplexFloat ) );

  if( time ) *time = priv->index_time;
  status = TRUE;

  exit:

  g_mutex_unlock( &priv->lock );

  return status;

}

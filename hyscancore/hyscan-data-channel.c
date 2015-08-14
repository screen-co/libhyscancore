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

#include <math.h>
#include <string.h>
#include "pffft.h"

#ifdef HYSCAN_CORE_USE_OPENMP
#include <omp.h>
#endif


#define DATA_CHANNEL_API       20150700             // Версия API канала гидролокационных данных.
#define SIGNALS_CHANNEL_NAME   "signals"            // Название канала данных с образцами излучавшихся сигналов.


enum { PROP_O, PROP_DB, PROP_PROJECT_NAME, PROP_TRACK_NAME, PROP_CHANNEL_NAME };


typedef struct HyScanDataChannelPriv {              // Внутренние данные объекта.

  HyScanDB                  *db;                    // Объект базы данных.

  gchar                     *project_name;          // Название проекта.
  gchar                     *track_name;            // Название галса.
  gchar                     *channel_name;          // Название канала данных.

  gint32                     channel_id;            // Идентификатор открытого канала данных.
  gint32                     param_id;              // Идентификатор открытой группы параметров канала данных.

  HyScanDataChannelInfo      info;                  // Информация о канале данных.

  gpointer                   buffer;                // Буфер для чтения данных канала.
  gint32                     buffer_size;           // Размер буфера, в байтах.

  PFFFT_Setup               *fft;                   // Коэффициенты преобразования Фурье.
  gint32                     fft_size;              // Размер преобразования Фурье.
  gfloat                     fft_scale;             // Коэффициент масштабирования свёртки.
  HyScanComplexFloat        *fft_signal;            // Образец сигнала для свёртки.

  HyScanComplexFloat        *ibuff;                 // Буфер для выполнения преобразований.
  HyScanComplexFloat        *obuff;                 // Буфер для выполнения преобразований.

  GMutex                     mutex;                 // Блокировка многопоточного доступа.

  gboolean                   time_measure;          // Включение измерения времени свёртки.
  GTimer                    *timer;                 // Таймер измерения времени свёртки.
  gint64                     last_time_output;      // Время крайнего отображения измерений.
  gint64                     n_avg_time_samples;    // Текущее число измерений.
  gint64                     n_total_time_samples;  // Общее число измерений.
  gdouble                    avg_time;              // Накопленное время измерений за последние n_avg_time_samples преобразований.
  gdouble                    total_time;            // Накопленное время измерений за все время.

  gboolean                   readonly;              // Объект в режиме только для чтения.
  gboolean                   fail;                  // Признак ошибки в объекте.

} HyScanDataChannelPriv;


#define HYSCAN_DATA_CHANNEL_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), G_TYPE_HYSCAN_DATA_CHANNEL, HyScanDataChannelPriv ) )


static void hyscan_data_channel_set_property( HyScanDataChannel *dchannel, guint prop_id, const GValue *value, GParamSpec *pspec );
static GObject* hyscan_data_channel_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties );
static void hyscan_data_channel_object_finalize( HyScanDataChannel *dchannel );


G_DEFINE_TYPE( HyScanDataChannel, hyscan_data_channel, G_TYPE_OBJECT );


static void hyscan_data_channel_init( HyScanDataChannel *dchannel ){ ; }


static void hyscan_data_channel_class_init( HyScanDataChannelClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->set_property = hyscan_data_channel_set_property;

  this_class->constructor = hyscan_data_channel_object_constructor;
  this_class->finalize = hyscan_data_channel_object_finalize;

  g_object_class_install_property( this_class, PROP_DB,
    g_param_spec_pointer( "db", "DB", "HyScanDB interface", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_PROJECT_NAME,
    g_param_spec_string( "project-name", "ProjectName", "Project Name", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_TRACK_NAME,
    g_param_spec_string( "track-name", "TrackName", "Track Name", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_object_class_install_property( this_class, PROP_CHANNEL_NAME,
    g_param_spec_string( "channel-name", "ChannelName", "Channel Name", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_type_class_add_private( klass, sizeof( HyScanDataChannelPriv ) );

}


static void hyscan_data_channel_set_property( HyScanDataChannel *dchannel, guint prop_id, const GValue *value, GParamSpec *pspec )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );
  HyScanDB *db;

  switch ( prop_id )
    {

    case PROP_DB:
      priv->db = NULL;
      db = g_value_get_pointer( value );
      if( db == NULL || !g_type_is_a( G_OBJECT_TYPE( db ), G_TYPE_HYSCAN_DB ) )
        G_OBJECT_WARN_INVALID_PROPERTY_ID( dchannel, prop_id, pspec );
      else
        priv->db = db;
      break;

    case PROP_PROJECT_NAME:
      priv->project_name = g_value_dup_string( value );
      break;

    case PROP_TRACK_NAME:
      priv->track_name = g_value_dup_string( value );
      break;

    case PROP_CHANNEL_NAME:
      priv->channel_name = g_value_dup_string( value );
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( dchannel, prop_id, pspec );
      break;

    }

}


// Конструктор объекта.
static GObject* hyscan_data_channel_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties )
{

  GObject *dchannel = G_OBJECT_CLASS( hyscan_data_channel_parent_class )->constructor( g_type, n_construct_properties, construct_properties );
  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gchar *type;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 signals_id = -1;

  // Начальные значения.
  priv->channel_id = -1;
  priv->param_id = -1;

  priv->info.discretization_type = HYSCAN_DATA_TYPE_UNKNOWN;
  priv->info.discretization_frequency = 0.0;

  priv->info.signal_type = HYSCAN_SIGNAL_TYPE_UNKNOWN;
  priv->info.signal_frequency = 0.0;
  priv->info.signal_spectrum = 0.0;
  priv->info.signal_duration = 0.0;

  priv->info.antenna_x_offset = 0.0;
  priv->info.antenna_y_offset = 0.0;
  priv->info.antenna_z_offset = 0.0;

  priv->info.antenna_roll_offset = 0.0;
  priv->info.antenna_pitch_offset = 0.0;
  priv->info.antenna_yaw_offset = 0.0;

  priv->info.antenna_base_offset = 0.0;
  priv->info.antenna_hpattern = 0.0;
  priv->info.antenna_vpattern = 0.0;

  priv->buffer = NULL;
  priv->buffer_size = 0;

  priv->fft = NULL;
  priv->fft_size = 32;
  priv->fft_scale = 1.0;
  priv->fft_signal = NULL;
  priv->ibuff = NULL;
  priv->obuff = NULL;

  if( g_getenv( "HYSCAN_CORE_TIME_MEASURE" ) != NULL || g_getenv( "TIME_MEASURE" ) != NULL )
    {
    priv->time_measure = TRUE;
    priv->timer = g_timer_new();
    priv->last_time_output = g_get_monotonic_time();
    priv->n_avg_time_samples = 0;
    priv->n_total_time_samples = 0;
    priv->avg_time = 0.0;
    priv->total_time = 0.0;
    }
  else
    {
    priv->time_measure = FALSE;
    priv->timer = NULL;
    }

  priv->readonly = TRUE;

  priv->fail = TRUE;

  g_mutex_init( &priv->mutex );

  // Проверяем, что передан указатель на базу данных.
  if( priv->db != NULL ) g_object_ref( priv->db );
  else goto exit;

  // Проверяем, что заданы имя проекта, галса и канала данных.
  if( priv->project_name == NULL || priv->track_name == NULL || priv->channel_name == NULL )
    {
    if( priv->project_name == NULL ) g_critical( "hyscan_data_channel_object_constructor: unknown project name" );
    if( priv->track_name == NULL ) g_critical( "hyscan_data_channel_object_constructor: unknown track name" );
    if( priv->channel_name == NULL ) g_critical( "hyscan_data_channel_object_constructor: unknown channel name" );
    goto exit;
    }

  // Открываем проект.
  project_id = hyscan_db_open_project( priv->db, priv->project_name );
  if( project_id < 0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: can't open project '%s'", priv->project_name );
    goto exit;
    }

  // Открываем галс.
  track_id = hyscan_db_open_track( priv->db, project_id, priv->track_name );
  if( track_id < 0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: can't open track '%s.%s'", priv->project_name, priv->track_name );
    goto exit;
    }

  // Открываем канал данных.
  priv->channel_id = hyscan_db_open_channel( priv->db, track_id, priv->channel_name );
  if( priv->channel_id < 0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: can't open channel '%s.%s.%s'", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Открываем группу параметров канала данных.
  priv->param_id = hyscan_db_open_channel_param( priv->db, priv->channel_id );
  if( priv->param_id < 0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: can't open channel '%s.%s.%s' parameters", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Проверяем версию API канала данных.
  if( (guint32)( hyscan_db_get_integer_param( priv->db, priv->param_id, "channel.version" ) / 100 ) != (guint32)( DATA_CHANNEL_API / 100 ) )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unsupported api version (%d)", priv->project_name, priv->track_name, priv->channel_name, (guint32)hyscan_db_get_integer_param( priv->db, priv->param_id, "channel.version" ) );
    goto exit;
    }

  // Тип дискретизации данных в канале.
  type = hyscan_db_get_string_param( priv->db, priv->param_id, "discretization.type" );
  priv->info.discretization_type = hyscan_core_get_data_type_by_name( type );
  g_free( type );
  if( priv->info.discretization_type == HYSCAN_DATA_TYPE_UNKNOWN )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unknown discretization type", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Частота дискретизации данных.
  priv->info.discretization_frequency = hyscan_db_get_double_param( priv->db, priv->param_id, "discretization.frequency" );
  if( priv->info.discretization_frequency < 1.0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unsupported discretization frequency %.3fHz", priv->project_name, priv->track_name, priv->channel_name, priv->info.discretization_frequency );
    goto exit;
    }

  // Тип рабочего сигнала.
  type = hyscan_db_get_string_param( priv->db, priv->param_id, "signal.type" );
  priv->info.signal_type = hyscan_core_get_signal_type_by_name( type );
  g_free( type );
  if( priv->info.signal_type == HYSCAN_SIGNAL_TYPE_UNKNOWN )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unknown signal type", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Рабочая (центральная) частота сигнала
  priv->info.signal_frequency = hyscan_db_get_double_param( priv->db, priv->param_id, "signal.frequency" );
  if( priv->info.signal_frequency < 1.0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unsupported signal frequency %.3fHz", priv->project_name, priv->track_name, priv->channel_name, priv->info.signal_frequency );
    goto exit;
    }

  // Ширина спектра сигнала.
  priv->info.signal_spectrum = hyscan_db_get_double_param( priv->db, priv->param_id, "signal.spectrum" );
  if( priv->info.signal_spectrum < 1.0 )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unsupported signal spectrum %.3fHz", priv->project_name, priv->track_name, priv->channel_name, priv->info.signal_spectrum );
    goto exit;
    }

  // Длительность сигнала
  priv->info.signal_duration = hyscan_db_get_double_param( priv->db, priv->param_id, "signal.duration" );
  if( priv->info.signal_duration < 1e-7 )
    {
    g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unsupported signal duration %.3fms", priv->project_name, priv->track_name, priv->channel_name, 1e3 * priv->info.signal_duration );
    goto exit;
    }

  // Смещения местоположения антенны.
  priv->info.antenna_x_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.x-offset" );
  priv->info.antenna_y_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.y-offset" );
  priv->info.antenna_z_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.z-offset" );

  // Углы установки антенны.
  priv->info.antenna_roll_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.roll_offset" );
  priv->info.antenna_pitch_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.pitch_offset" );
  priv->info.antenna_yaw_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.yaw_offset" );

  // Характеристики антенны.
  priv->info.antenna_base_offset = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.base-offset" );
  priv->info.antenna_hpattern = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.hpattern" );
  priv->info.antenna_vpattern = hyscan_db_get_double_param( priv->db, priv->param_id, "antenna.vpattern" );

  // Проверяем необходимость использования образца сигнала для свёртки.
  if( hyscan_db_has_param( priv->db, priv->param_id, "signal.index" ) )
    {

    gint signal_image_index;

    HyScanComplexFloat *signal;
    gint32 n_signal_points;
    gint32 signal_size;

    // Образцы сигналов для свёртки.
    signals_id = hyscan_db_open_channel( priv->db, track_id, SIGNALS_CHANNEL_NAME );
    if( signals_id < 0 )
      {
      g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': can't open signals", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Размер образца сигнала для свёртки.
    signal_image_index = hyscan_db_get_integer_param( priv->db, priv->param_id, "signal.index" );
    if( !hyscan_db_get_channel_data( priv->db, signals_id, signal_image_index, NULL, &signal_size, NULL ) )
      {
      g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': can't get signal image size", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Размер образца сигнала должен быть кратен размеру структуры HyScanComplexFloat.
    if( ( signal_size % sizeof( HyScanComplexFloat ) ) != 0 )
      {
      g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': unsupported signal image size", priv->project_name, priv->track_name, priv->channel_name );
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
    if( !hyscan_data_channel_set_signal_image( dchannel, signal, n_signal_points ) )
      {
      g_free( signal );
      g_critical( "hyscan_data_channel_object_constructor: '%s.%s.%s': can't set signal image", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    g_free( signal );

    }

  // Закончили создание объекта.
  priv->fail = FALSE;

  exit:

  if( signals_id > 0 ) hyscan_db_close_param( priv->db, signals_id );
  if( project_id > 0 ) hyscan_db_close_project( priv->db, project_id );
  if( track_id > 0 ) hyscan_db_close_track( priv->db, track_id );

  return dchannel;

}


// Деструктор объекта.
static void hyscan_data_channel_object_finalize( HyScanDataChannel *dchannel )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  if( priv->time_measure && priv->n_total_time_samples > 0 )
    g_info( "hyscan_data_channel_object_finalize: '%s.%s.%s': average convolution time: %.06lfs", priv->project_name, priv->track_name, priv->channel_name, priv->total_time / priv->n_total_time_samples );

  if( priv->fft != NULL ) pffft_destroy_setup( priv->fft );

  if( priv->fft_signal != NULL ) pffft_aligned_free( priv->fft_signal );
  if( priv->ibuff != NULL ) pffft_aligned_free( priv->ibuff );
  if( priv->obuff != NULL ) pffft_aligned_free( priv->obuff );
  g_free( priv->buffer );

  g_free( priv->project_name );
  g_free( priv->track_name );
  g_free( priv->channel_name );

  if( priv->channel_id > 0 ) hyscan_db_close_channel( priv->db, priv->channel_id );
  if( priv->param_id > 0 ) hyscan_db_close_param( priv->db, priv->param_id );

  if( priv->timer != NULL ) g_timer_destroy( priv->timer );

  g_mutex_clear( &priv->mutex );

  g_object_unref( priv->db );

}


// Функция выделяет память под буферы увеличенного размера.
static void hyscan_data_channel_buffer_realloc( HyScanDataChannelPriv *priv, gint32 new_buffer_size )
{

  gint n_points;
  gint n_fft;

  // Запас для проверки помещаются все данные в буфер или нет.
  new_buffer_size += 32;

  // Изменяем размер буфера.
  priv->buffer = g_realloc( priv->buffer, new_buffer_size );
  priv->buffer_size = new_buffer_size;

  // Число отсчётов данных.
  n_points = new_buffer_size / hyscan_core_get_data_point_size( priv->info.discretization_type );

  // Число блоков преобразования Фурье над одной строкой.
  n_fft = ( n_points / ( priv->fft_size / 2 ) );
  if( ( n_points % ( priv->fft_size / 2 ) ) ) n_fft += 1;

  // Освобождаем память старых буферов.
  if( priv->ibuff != NULL ) pffft_aligned_free( priv->ibuff );
  if( priv->obuff != NULL ) pffft_aligned_free( priv->obuff );

  // Память под новые буферы.
  priv->ibuff = pffft_aligned_malloc( n_fft * priv->fft_size * sizeof( HyScanComplexFloat ) );
  priv->obuff = pffft_aligned_malloc( n_fft * priv->fft_size * sizeof( HyScanComplexFloat ) );

}


// Функция выполняет свертку для данных с числом точек = n_points.
static void hyscan_data_channel_convolve( HyScanDataChannelPriv *priv, gint32 n_points )
{

  // Свертка выполняется блоками по priv->fft_size элементов, при этом каждый следующий блок смещен относительно предыдущего
  // на priv->fft_size / 2 элементов. Входные данные находятся в priv->ibuff, где над ними производится прямое преобразование
  // Фурье с сохранением результата в priv->obuff, но уже без перекрытия, т.е. с шагом priv->fft_size. Затем производится
  // перемножение ("свертка") с нужным образцом сигнала и обратное преобразование Фурье в priv->ibuff.
  // Таким образом в priv->obuff оказываются необходимые данные, разбитые на некоторое число блоков, в каждом из которых нам
  // нужны только первые priv->fft_size / 2 элементов.

  // Так как операции над блоками происходят независимо друг от друга это процесс можно выполнять параллельно, что и производится
  // за счет использования библиотеки OpenMP.

  gint i;
  gint n_fft;

  gint full_size = priv->fft_size;
  gint half_size = priv->fft_size / 2;

  // Свёртка не нужна.
  if( priv->fft == NULL || priv->fft_signal == NULL ) return;

  // Измерение скорости выполнения свёртки.
  if( priv->time_measure ) g_timer_start( priv->timer );

  // Число блоков преобразования Фурье над одной строкой.
  n_fft = ( n_points / half_size );
  if( n_points % half_size ) n_fft += 1;

  // Зануляем конец буфера по границе half_size.
  memset( priv->ibuff + n_points, 0, ( ( n_fft + 1 ) * half_size - n_points ) * sizeof( HyScanComplexFloat ) );

  // Прямое преобразование Фурье.
#ifdef HYSCAN_CORE_USE_OPENMP
  #pragma omp parallel for
#endif
  for( i = 0; i < n_fft; i++ )
    pffft_transform( priv->fft, (const gfloat*)( priv->ibuff + ( i * half_size ) ), (gfloat*)( priv->obuff + ( i * full_size ) ), NULL, PFFFT_FORWARD );

  // Свёртка и обратное преобразование Фурье.
#ifdef HYSCAN_CORE_USE_OPENMP
  #pragma omp parallel for
#endif
  for( i = 0; i < n_fft; i++ )
    {

    guint offset = i * full_size;

    // Обнуляем выходной буфер, т.к. функция zconvolve_accumulate добавляет полученный результат
    // к значениям в этом буфере (нам это не нужно) и выполняем свёртку.
    memset( priv->ibuff + offset, 0, full_size * sizeof( HyScanComplexFloat ) );
    pffft_zconvolve_accumulate( priv->fft, (const gfloat*)( priv->obuff + offset ), (const gfloat*)priv->fft_signal, (gfloat*)( priv->ibuff + offset ), priv->fft_scale );

    // Выполняем обратное преобразование Фурье.
    pffft_zreorder( priv->fft, (gfloat*)( priv->ibuff + offset ), (gfloat*)( priv->obuff + offset ), PFFFT_FORWARD );
    pffft_transform_ordered( priv->fft, (gfloat*)( priv->obuff + offset ), (gfloat*)( priv->ibuff + offset ), NULL, PFFFT_BACKWARD );

    }

  // Измерение скорости выполнения свёртки.
  if( priv->time_measure )
    {

    gdouble time = g_timer_elapsed( priv->timer, NULL );
    gint64 ctime = g_get_monotonic_time();

    priv->n_avg_time_samples += 1;
    priv->n_total_time_samples += 1;

    priv->avg_time += time;
    priv->total_time += time;

    // Раз в секунду отображаем результат измерений.
    if( ctime - priv->last_time_output > G_USEC_PER_SEC )
      {
      priv->last_time_output = ctime;
      g_info( "hyscan_data_channel_convolve: '%s.%s.%s': average time: %.06lfs, total average time: %.06lfs", priv->project_name, priv->track_name, priv->channel_name, priv->avg_time / priv->n_avg_time_samples, priv->total_time / priv->n_total_time_samples );
      priv->n_avg_time_samples = 0;
      priv->avg_time = 0.0;
      }

    }

}


HyScanDataChannelInfo *hyscan_data_channel_get_info( HyScanDataChannel *dchannel )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );
  HyScanDataChannelInfo *info = g_malloc( sizeof( HyScanDataChannelInfo ) );

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

  return info;

}


// Функция создаёт новый канал хранения акустических данных и открывает его для первичной обработки.
HyScanDataChannel *hyscan_data_channel_create( HyScanDB *db, const gchar *project_name, const gchar *track_name, const gchar *channel_name, HyScanDataChannelInfo *info, gint32 signal_index )
{

  HyScanDataChannel *dchannel = NULL;
  HyScanDataChannelPriv *priv;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 channel_id = -1;
  gint32 param_id = -1;

  // Открываем проект.
  project_id = hyscan_db_open_project( db, project_name );
  if( project_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't open project '%s'", project_name );
    goto exit;
    }

  // Открываем галс.
  track_id = hyscan_db_open_track( db, project_id, track_name );
  if( track_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't open track '%s.%s'", project_name, track_name );
    goto exit;
    }

  // Создаём канал данных.
  channel_id = hyscan_db_create_channel( db, track_id, channel_name );
  if( track_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't create channel '%s.%s.%s'", project_name, track_name, channel_name );
    goto exit;
    }

  // Параметры канала данных.
  param_id = hyscan_db_open_channel_param( db, channel_id );
  if( param_id < 0 )
    {
    g_critical( "hyscan_data_channel_create: can't set channel '%s.%s.%s' parameters", project_name, track_name, channel_name );
    goto exit;
    }

  hyscan_db_set_string_param( db, param_id, "channel.version", "20150700" );

  hyscan_db_set_string_param( db, param_id, "discretization.type", hyscan_core_get_data_type_name( info->discretization_type ) );
  hyscan_db_set_double_param( db, param_id, "discretization.frequency", info->discretization_frequency );

  hyscan_db_set_string_param( db, param_id, "signal.type", hyscan_core_get_signal_type_name( info->signal_type ) );
  hyscan_db_set_double_param( db, param_id, "signal.frequency", info->signal_frequency );
  hyscan_db_set_double_param( db, param_id, "signal.spectrum", info->signal_spectrum );
  hyscan_db_set_double_param( db, param_id, "signal.duration", info->signal_duration );
  if( signal_index >= 0 ) hyscan_db_set_integer_param( db, param_id, "signal.index", signal_index );

  hyscan_db_set_double_param( db, param_id, "antenna.x-offset", info->antenna_x_offset );
  hyscan_db_set_double_param( db, param_id, "antenna.y-offset", info->antenna_y_offset );
  hyscan_db_set_double_param( db, param_id, "antenna.z-offset", info->antenna_z_offset );

  hyscan_db_set_double_param( db, param_id, "antenna.roll-offset", info->antenna_roll_offset );
  hyscan_db_set_double_param( db, param_id, "antenna.pitch-offset", info->antenna_pitch_offset );
  hyscan_db_set_double_param( db, param_id, "antenna.yaw-offset", info->antenna_yaw_offset );

  hyscan_db_set_double_param( db, param_id, "antenna.base-offset", info->antenna_base_offset );
  hyscan_db_set_double_param( db, param_id, "antenna.hpattern", info->antenna_hpattern );
  hyscan_db_set_double_param( db, param_id, "antenna.vpattern", info->antenna_vpattern );

  // Открываем канал данных и отключаем режим только чтения.
  dchannel = hyscan_data_channel_open( db, project_name, track_name, channel_name );
  if( dchannel != NULL )
    {
    priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );
    priv->readonly = FALSE;
    }

  exit:

  if( param_id > 0 ) hyscan_db_close_param( db, param_id );
  if( channel_id > 0 ) hyscan_db_close_channel( db, channel_id );
  if( track_id > 0 ) hyscan_db_close_track( db, track_id );
  if( project_id > 0 ) hyscan_db_close_project( db, project_id );

  return dchannel;

}


// Функция открывает существующий канал хранения акустических данных для первичной обработки.
HyScanDataChannel *hyscan_data_channel_open( HyScanDB *db, const gchar *project_name, const gchar *track_name, const gchar *channel_name )
{

  HyScanDataChannel *dchannel = g_object_new( G_TYPE_HYSCAN_DATA_CHANNEL, "db", db, "project-name", project_name, "track-name", track_name, "channel-name", channel_name, NULL );
  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  if( !priv->fail ) return dchannel;

  g_object_unref( dchannel );

  return NULL;

}


// Функция задаёт новый образец сигнала для свёртки.
gboolean hyscan_data_channel_set_signal_image( HyScanDataChannel *dchannel, HyScanComplexFloat *signal, gint32 n_signal_points )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;

  HyScanComplexFloat *signal_buff;

  gint32 conv_size = 2 * n_signal_points;
  gint32 opt_size = 0;

  gint i, j, k;

  g_mutex_lock( &priv->mutex );

  // Отменяем свёртку с текущим сигналом.
  if( priv->fft != NULL ) pffft_destroy_setup( priv->fft );
  if( priv->fft_signal != NULL ) pffft_aligned_free( priv->fft_signal );
  priv->fft = NULL;
  priv->fft_signal = NULL;

  // Пользователь отменил свёртку.
  if( signal == NULL ) { status = TRUE; goto exit; }

  // Ищем оптимальный размер свёртки для библиотеки pffft_new_setup (см. pffft.h).
  opt_size = G_MAXINT32;
  for( i = 32; i <= 512; i *= 2 )
    for( j = 1; j <= 243; j *= 3 )
      for( k = 1; k <=5 ; k *= 5 )
        {
        if( i * j * k < conv_size ) continue;
        if( ABS( i * j * k - conv_size ) < ABS( opt_size - conv_size ) ) opt_size = i * j * k;
        }

  if( opt_size == G_MAXINT32 )
    {
    g_critical( "hyscan_data_channel_set_signal_image: '%s.%s.%s': can't find optimal fft size", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  priv->fft_size = opt_size;

  // Коэффициент масштабирования свёртки.
  priv->fft_scale = 1.0 / ( (gfloat)priv->fft_size * (gfloat)n_signal_points );

  // Параметры преобразования Фурье.
  priv->fft = pffft_new_setup( priv->fft_size, PFFFT_COMPLEX );
  if( priv->fft == NULL )
    {
    g_critical( "hyscan_data_channel_set_signal_image: '%s.%s.%s': can't setup fft", priv->project_name, priv->track_name, priv->channel_name );
    goto exit;
    }

  // Копируем образец сигнала.
  priv->fft_signal = pffft_aligned_malloc( priv->fft_size * sizeof( HyScanComplexFloat ) );
  memset( priv->fft_signal, 0, priv->fft_size * sizeof( HyScanComplexFloat ) );
  memcpy( priv->fft_signal, signal, n_signal_points * sizeof( HyScanComplexFloat ) );

  // Подготавливаем образец к свёртке и делаем его комплексно сопряжённым.
  signal_buff = pffft_aligned_malloc( priv->fft_size * sizeof( HyScanComplexFloat ) );
  pffft_transform_ordered( priv->fft, (const gfloat*)priv->fft_signal, (gfloat*)signal_buff, NULL, PFFFT_FORWARD );
  for( i = 0; i < priv->fft_size; i++ ) signal_buff[i].im = -signal_buff[i].im;
  pffft_zreorder( priv->fft, (const gfloat*)signal_buff, (gfloat*)priv->fft_signal, PFFFT_BACKWARD );
  pffft_aligned_free( signal_buff );

  // Если буферы для преобразований уже выделены, изменяем их размер.
  if( priv->buffer_size > 0 ) hyscan_data_channel_buffer_realloc( priv, priv->buffer_size );

  status = TRUE;

  exit:

  g_mutex_unlock( &priv->mutex );

  return status;

}


// Функция возвращает диапазон значений индексов записанных данных.
gboolean hyscan_data_channel_get_range( HyScanDataChannel *dchannel, gint32 *first_index, gint32 *last_index )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  // Проверка состояния объекта.
  if( priv->fail ) return FALSE;

  return hyscan_db_get_channel_data_range( priv->db, priv->channel_id, first_index, last_index );

}


// Функция записывает новые данные в канал.
gboolean hyscan_data_channel_add_data( HyScanDataChannel *dchannel, gint64 time, gpointer data, gint32 size, gint32 *index )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  // Проверка состояния объекта.
  if( priv->fail || priv->readonly ) return FALSE;

  return hyscan_db_add_channel_data( priv->db, priv->channel_id, time, data, size, index );

}


// Функция возвращает значения амплитуды акустического сигнала.
gboolean hyscan_data_channel_get_amplitude_values( HyScanDataChannel *dchannel, gboolean convolve, gint32 index, gfloat *buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;
  gint32 io_size = priv->buffer_size;
  gint32 n_points;
  gint32 i, j;

  // Проверка состояния объекта.
  if( priv->fail ) return FALSE;

  // Если запрошен только размер данных и время записи.
  if( buffer == NULL )
    {
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, NULL, buffer_size, time ) ) return FALSE;
    *buffer_size /= hyscan_core_get_data_point_size( priv->info.discretization_type );
    return TRUE;
    }

  g_mutex_lock( &priv->mutex );

  // Считываем "сырые" данные канала.
  if( !hyscan_data_channel_get_raw_values( dchannel, index, priv->buffer, &io_size, time ) ) goto exit;

  // Преобразовываем "сырые" данные в HyScanComplexFloat.
  n_points = hyscan_core_import_data( priv->info.discretization_type, priv->ibuff, priv->buffer, io_size );
  if( n_points < 0 ) goto exit;

  // Выполняем свёртку.
  if( convolve ) hyscan_data_channel_convolve( priv, n_points );

  // Возвращаем амплитуду.
  *buffer_size = n_points = ( *buffer_size < n_points ) ? *buffer_size : n_points;
  for( i = 0; i < n_points; i++ )
    {
    if( convolve ) j = ( ( i / ( priv->fft_size / 2 ) ) * priv->fft_size ) + ( i % ( priv->fft_size / 2 ) );
    else j = i;
    buffer[i] = sqrtf( priv->ibuff[j].re * priv->ibuff[j].re + priv->ibuff[j].im * priv->ibuff[j].im );
    }

  status = TRUE;

  exit:

  g_mutex_unlock( &priv->mutex );

  return status;

}


// Функция возвращает квадратурные отсчёты акустического сигнала.
gboolean hyscan_data_channel_get_quadrature_values( HyScanDataChannel *dchannel, gboolean convolve, gint32 index, HyScanComplexFloat *buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;
  gint32 io_size = priv->buffer_size;
  gint32 n_points;
  gint32 i, j;

  // Проверка состояния объекта.
  if( priv->fail ) return FALSE;

  // Если запрошен только размер данных и время записи.
  if( buffer == NULL )
    {
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, NULL, buffer_size, time ) ) return FALSE;
    *buffer_size /= hyscan_core_get_data_point_size( priv->info.discretization_type );
    return TRUE;
    }

  g_mutex_lock( &priv->mutex );

  // Считываем "сырые" данные канала.
  if( !hyscan_data_channel_get_raw_values( dchannel, index, priv->buffer, &io_size, time ) ) goto exit;

  // Преобразовываем "сырые" данные в HyScanComplexFloat.
  n_points = hyscan_core_import_data( priv->info.discretization_type, priv->ibuff, priv->buffer, io_size );
  if( n_points < 0 ) goto exit;

  // Выполняем свёртку.
  if( convolve ) hyscan_data_channel_convolve( priv, n_points );

  // Возвращаем квадратурные отсчёты.
  *buffer_size = n_points = ( *buffer_size < n_points ) ? *buffer_size : n_points;
  for( i = 0; i < n_points; i++ )
    {
    if( convolve ) j = ( ( i / ( priv->fft_size / 2 ) ) * priv->fft_size ) + ( i % ( priv->fft_size / 2 ) );
    else j = i;
    buffer[i] = priv->ibuff[j];
    }

  status = TRUE;

  exit:

  g_mutex_unlock( &priv->mutex );

  return status;

}


// Функция возвращает фазу или разность фаз акустического сигнала.
gboolean hyscan_data_channel_get_phase_values( HyScanDataChannel *dchannel, HyScanDataChannel *pair, gboolean convolve, gint32 index, gfloat *buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  gboolean status = FALSE;
  gint32 io_size = priv->buffer_size;
  gint32 n_points1;
  gint64 time1;
  gint32 i, j;

  // Проверка состояния объекта.
  if( priv->fail ) return FALSE;

  // Если запрошен только размер данных и время записи.
  if( buffer == NULL )
    {
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, NULL, buffer_size, time ) ) return FALSE;
    *buffer_size /= hyscan_core_get_data_point_size( priv->info.discretization_type );
    return TRUE;
    }

  g_mutex_lock( &priv->mutex );

  // Считываем "сырые" данные канала.
  if( !hyscan_data_channel_get_raw_values( dchannel, index, priv->buffer, &io_size, &time1 ) ) goto exit;

  // Преобразовываем "сырые" данные в HyScanComplexFloat.
  n_points1 = hyscan_core_import_data( priv->info.discretization_type, priv->ibuff, priv->buffer, io_size );
  if( n_points1 < 0 ) goto exit;

  // Выполняем свёртку.
  if( convolve ) hyscan_data_channel_convolve( priv, n_points1 );

  // Считываем данные второго канала.
  if( pair != NULL )
    {

    gint32 index2;
    gint32 n_points2;
    gint64 time2;

    // Нельзя использовать самого себя...
    if( pair == dchannel )
      {
      g_critical( "hyscan_data_channel_get_phase_values: '%s.%s.%s': pair channel is the same", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Проверяем тип объекта второго канала данных.
    if( !g_type_is_a( G_OBJECT_TYPE( pair ), G_TYPE_HYSCAN_DATA_CHANNEL ) )
      {
      g_critical( "hyscan_data_channel_get_phase_values: '%s.%s.%s': incorrect pair channel object type", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Сверяем метку времени данных.
    if( !hyscan_data_channel_find_data( pair, time1, &index2, NULL, &time2, NULL ) || time1 != time2 )
      {
      g_critical( "hyscan_data_channel_get_phase_values: '%s.%s.%s': can't find paired quadrature values", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    // Считываем данные второго канала.
    n_points2 = n_points1;
    if( !hyscan_data_channel_get_quadrature_values( pair, convolve, index2, priv->obuff, &n_points2, NULL ) || n_points1 != n_points2 )
      {
      g_critical( "hyscan_data_channel_get_phase_values: '%s.%s.%s': can't get paired quadrature values", priv->project_name, priv->track_name, priv->channel_name );
      goto exit;
      }

    }

  // Возвращаем значение фазы.
  *buffer_size = n_points1 = ( *buffer_size < n_points1 ) ? *buffer_size : n_points1;
  for( i = 0; i < n_points1; i++ )
    {
    gfloat re, im;
    if( convolve ) j = ( ( i / ( priv->fft_size / 2 ) ) * priv->fft_size ) + ( i % ( priv->fft_size / 2 ) );
    else j = i;
    if( pair == NULL )
      {
      re = priv->ibuff[j].re;
      im = priv->ibuff[j].im;
      }
    else
      {
      re = priv->ibuff[j].re * priv->obuff[i].re + priv->ibuff[j].im * priv->obuff[i].im;
      im = priv->obuff[i].re * priv->ibuff[j].im - priv->ibuff[j].re * priv->obuff[i].im;
      }
    buffer[i] = atan2f( im, re );
    }

  status = TRUE;

  exit:

  g_mutex_unlock( &priv->mutex );

  return status;

}


// Функция возвращает необработанные акустические данные в формате их записи.
gboolean hyscan_data_channel_get_raw_values( HyScanDataChannel *dchannel, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  // Проверка состояния объекта.
  if( priv->fail ) return FALSE;

  // Считываем данные канала.
  if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, buffer, buffer_size, time ) ) return FALSE;

  // Если функция вызывалась пользователем библиотеки - возвращаем результат.
  if( buffer != priv->buffer ) return TRUE;

  // Если размер считанных данных равен размеру буфера, запрашиваем реальный размер,
  // увеличиваем размер буфера и считываем данные еще раз.
  if( *buffer_size == priv->buffer_size || priv->buffer_size == 0 )
    {
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, NULL, buffer_size, NULL ) ) return FALSE;
    hyscan_data_channel_buffer_realloc( priv, *buffer_size );
    *buffer_size = priv->buffer_size;
    if( !hyscan_db_get_channel_data( priv->db, priv->channel_id, index, priv->buffer, buffer_size, time ) ) return FALSE;
    }

  return TRUE;

}


// Функция ищет индекс данных для указанного момента времени.
gboolean hyscan_data_channel_find_data( HyScanDataChannel *dchannel, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime )
{

  HyScanDataChannelPriv *priv = HYSCAN_DATA_CHANNEL_GET_PRIVATE( dchannel );

  // Проверка состояния объекта.
  if( priv->fail ) return FALSE;

  return hyscan_db_find_channel_data( priv->db, priv->channel_id, time, lindex, rindex, ltime, rtime );

}

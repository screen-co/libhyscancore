/*!
 * \file hyscan-core-types.c
 *
 * \brief Исходный файл вспомогательных функций с определениями типов данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-core-types.h"


typedef struct HyScanCoreDataType {

  GQuark                     quark;
  const gchar               *name;
  HyScanDataType             type;

} HyScanCoreDataType;


typedef struct HyScanCoreSignalType {

  GQuark                     quark;
  const gchar               *name;
  HyScanSignalType           type;

} HyScanCoreSignalType;


// Типы данных и их названия.
static HyScanCoreDataType hyscan_core_data_types[] = {

  { 0, "HyScan-ADC12bit", HYSCAN_DATA_TYPE_ADC_12BIT },
  { 0, "HyScan-ADC14bit", HYSCAN_DATA_TYPE_ADC_14BIT },
  { 0, "HyScan-ADC16bit", HYSCAN_DATA_TYPE_ADC_16BIT },

  { 0, "HyScan-Complex-ADC12bit", HYSCAN_DATA_TYPE_COMPLEX_ADC_12BIT },
  { 0, "HyScan-Complex-ADC14bit", HYSCAN_DATA_TYPE_COMPLEX_ADC_14BIT },
  { 0, "HyScan-Complex-ADC16bit", HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT },

  { 0, "HyScan-Int8",   HYSCAN_DATA_TYPE_INT8 },
  { 0, "HyScan-UInt8",  HYSCAN_DATA_TYPE_UINT8 },
  { 0, "HyScan-Int16",  HYSCAN_DATA_TYPE_INT16 },
  { 0, "HyScan-UInt16", HYSCAN_DATA_TYPE_UINT16 },
  { 0, "HyScan-Int32",  HYSCAN_DATA_TYPE_INT32 },
  { 0, "HyScan-UInt32", HYSCAN_DATA_TYPE_UINT32 },
  { 0, "HyScan-Int64",  HYSCAN_DATA_TYPE_INT64 },
  { 0, "HyScan-UInt64", HYSCAN_DATA_TYPE_UINT64 },

  { 0, "HyScan-Complex-Int8",   HYSCAN_DATA_TYPE_COMPLEX_INT8 },
  { 0, "HyScan-Complex-UInt8",  HYSCAN_DATA_TYPE_COMPLEX_UINT8 },
  { 0, "HyScan-Complex-Int16",  HYSCAN_DATA_TYPE_COMPLEX_INT16 },
  { 0, "HyScan-Complex-UInt16", HYSCAN_DATA_TYPE_COMPLEX_UINT16 },
  { 0, "HyScan-Complex-Int32",  HYSCAN_DATA_TYPE_COMPLEX_INT32 },
  { 0, "HyScan-Complex-UInt32", HYSCAN_DATA_TYPE_COMPLEX_UINT32 },
  { 0, "HyScan-Complex-Int64",  HYSCAN_DATA_TYPE_COMPLEX_INT64 },
  { 0, "HyScan-Complex-UInt64", HYSCAN_DATA_TYPE_COMPLEX_UINT64 },

  { 0, "HyScan-Float",  HYSCAN_DATA_TYPE_FLOAT },
  { 0, "HyScan-Double", HYSCAN_DATA_TYPE_DOUBLE },

  { 0, "HyScan-Complex-Float",  HYSCAN_DATA_TYPE_COMPLEX_FLOAT },
  { 0, "HyScan-Complex-Double", HYSCAN_DATA_TYPE_COMPLEX_DOUBLE },

  { 0, NULL, HYSCAN_DATA_TYPE_UNKNOWN }

};


// Типы сигналов и их названия.
static HyScanCoreSignalType hyscan_core_signal_types[] = {

  { 0, "TONE", HYSCAN_SIGNAL_TYPE_TONE },
  { 0, "LFM",  HYSCAN_SIGNAL_TYPE_LFM },
  { 0, "LFMD", HYSCAN_SIGNAL_TYPE_LFMD },

  { 0, NULL, HYSCAN_SIGNAL_TYPE_UNKNOWN }

};


// Функция инициализации статических данных.
static void hyscan_core_initialize( void )
{

  static gboolean hyscan_core_initialized = FALSE;
  gint i;

  if( hyscan_core_initialized ) return;

  for( i = 0; hyscan_core_data_types[i].name != NULL; i++ )
    hyscan_core_data_types[i].quark = g_quark_from_static_string( hyscan_core_data_types[i].name );

  for( i = 0; hyscan_core_signal_types[i].name != NULL; i++ )
    hyscan_core_signal_types[i].quark = g_quark_from_static_string( hyscan_core_signal_types[i].name );

  hyscan_core_initialized = TRUE;

}


// Функция преобразовывает строку с названием типа сигнала в нумерованное значение.
HyScanSignalType hyscan_core_get_signal_type_by_name( const gchar *signal_name )
{

  GQuark quark;
  gint i;

  // Инициализация статических данных.
  hyscan_core_initialize();

  // Ищем строку с указанным именем типа сигнала.
  quark = g_quark_try_string( signal_name );
  for( i = 0; hyscan_core_signal_types[i].name != NULL; i++ )
    if( hyscan_core_signal_types[i].quark == quark )
      return hyscan_core_signal_types[i].type;

  return HYSCAN_SIGNAL_TYPE_UNKNOWN;

}


// Функция преобразовывает нумерованное значение типа данных в название типа.
const gchar *hyscan_core_get_signal_type_name( HyScanSignalType signal_type )
{

  gint i;

  // Инициализация статических данных.
  hyscan_core_initialize();

  // Ищем строку с указанным типом сигнала.
  for( i = 0; hyscan_core_signal_types[i].name != NULL; i++ )
    if( hyscan_core_signal_types[i].type == signal_type )
      return hyscan_core_signal_types[i].name;

  return NULL;

}


// Функция преобразовывает строку с названием типа данных в нумерованное значение.
HyScanDataType hyscan_core_get_data_type_by_name( const gchar *data_name )
{

  GQuark quark;
  gint i;

  // Инициализация статических данных.
  hyscan_core_initialize();

  // Ищем строку с указанным именем типа данных.
  quark = g_quark_try_string( data_name );
  for( i = 0; hyscan_core_data_types[i].name != NULL; i++ )
    if( hyscan_core_data_types[i].quark == quark )
      return hyscan_core_data_types[i].type;

  return HYSCAN_DATA_TYPE_UNKNOWN;

}


// Функция преобразовывает нумерованное значение типа данных в название типа.
const gchar *hyscan_core_get_data_type_name( HyScanDataType data_type )
{

  gint i;

  // Инициализация статических данных.
  hyscan_core_initialize();

  // Ищем строку с указанным типом данных.
  for( i = 0; hyscan_core_data_types[i].name != NULL; i++ )
    if( hyscan_core_data_types[i].type == data_type )
      return hyscan_core_data_types[i].name;

  return NULL;

}


// Функция возвращает размер одного отсчёта данных в байтах, для указанного типа.
guint32 hyscan_core_get_data_point_size( HyScanDataType data_type )
{

  switch( data_type )
    {

    case HYSCAN_DATA_TYPE_ADC_12BIT:
    case HYSCAN_DATA_TYPE_ADC_14BIT:
    case HYSCAN_DATA_TYPE_ADC_16BIT:         return sizeof( gint16 );

    case HYSCAN_DATA_TYPE_COMPLEX_ADC_12BIT:
    case HYSCAN_DATA_TYPE_COMPLEX_ADC_14BIT:
    case HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT: return 2 * sizeof( gint16 );

    case HYSCAN_DATA_TYPE_INT8:
    case HYSCAN_DATA_TYPE_UINT8:             return sizeof( gint8 );
    case HYSCAN_DATA_TYPE_INT16:
    case HYSCAN_DATA_TYPE_UINT16:            return sizeof( gint16 );
    case HYSCAN_DATA_TYPE_INT32:
    case HYSCAN_DATA_TYPE_UINT32:            return sizeof( gint32 );
    case HYSCAN_DATA_TYPE_INT64:
    case HYSCAN_DATA_TYPE_UINT64:            return sizeof( gint64 );

    case HYSCAN_DATA_TYPE_COMPLEX_INT8:
    case HYSCAN_DATA_TYPE_COMPLEX_UINT8:     return 2 * sizeof( gint8 );
    case HYSCAN_DATA_TYPE_COMPLEX_INT16:
    case HYSCAN_DATA_TYPE_COMPLEX_UINT16:    return 2 * sizeof( gint16 );
    case HYSCAN_DATA_TYPE_COMPLEX_INT32:
    case HYSCAN_DATA_TYPE_COMPLEX_UINT32:    return 2 * sizeof( gint32 );
    case HYSCAN_DATA_TYPE_COMPLEX_INT64:
    case HYSCAN_DATA_TYPE_COMPLEX_UINT64:    return 2 * sizeof( gint64 );

    case HYSCAN_DATA_TYPE_FLOAT:             return sizeof( gfloat );
    case HYSCAN_DATA_TYPE_DOUBLE:            return sizeof( gdouble );

    case HYSCAN_DATA_TYPE_COMPLEX_FLOAT:     return 2 * sizeof( gfloat );
    case HYSCAN_DATA_TYPE_COMPLEX_DOUBLE:    return 2 * sizeof( gdouble );

    default: break;

    }

  return 0;

}


// Функция преобразовывает данные из низкоуровневого формата в HyScanComplexFloat размером raw_size.
gint32 hyscan_core_import_data( HyScanDataType data_type, HyScanComplexFloat *data, gpointer raw, guint32 raw_size )
{

  guint32 i;
  guint32 n_points;

  switch( data_type )
    {

    case HYSCAN_DATA_TYPE_ADC_12BIT:
      n_points = raw_size / sizeof( gint16 );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + n_points ) ) & 0x0fff;
        data[i].re = (gfloat)raw_re / 4096.0;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_ADC_14BIT:
      n_points = raw_size / sizeof( gint16 );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + n_points ) ) & 0x3fff;
        data[i].re = (gfloat)raw_re / 16384.0;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_ADC_16BIT:
      n_points = raw_size / sizeof( gint16 );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / 32768.0;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_ADC_12BIT:
      n_points = raw_size / ( 2 * sizeof( gint16 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + 2 * n_points ) ) & 0x0fff;
        gint16 raw_im = GINT16_FROM_LE( *( (gint16*)raw + 2 * n_points + 1 ) ) & 0x0fff;
        data[i].re = (gfloat)raw_re / 4096.0;
        data[i].im = (gfloat)raw_im / 4096.0;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_ADC_14BIT:
      n_points = raw_size / ( 2 * sizeof( gint16 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + 2 * n_points ) ) & 0x3fff;
        gint16 raw_im = GINT16_FROM_LE( *( (gint16*)raw + 2 * n_points + 1 ) ) & 0x3fff;
        data[i].re = (gfloat)raw_re / 16384.0;
        data[i].im = (gfloat)raw_im / 16384.0;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT:
      n_points = raw_size / ( 2 * sizeof( gint16 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + 2 * i ) );
        gint16 raw_im = GINT16_FROM_LE( *( (gint16*)raw + 2 * i + 1 ) );
        data[i].re = (gfloat)raw_re / 32768.0;
        data[i].im = (gfloat)raw_im / 32768.0;
        }
      break;

    case HYSCAN_DATA_TYPE_INT8:
      n_points = raw_size / sizeof( gint8 );
      for( i = 0; i < n_points; i++ )
        {
        gint8 raw_re = *( (gint8*)raw + n_points );
        data[i].re = (gfloat)raw_re / G_MAXINT8;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_UINT8:
      n_points = raw_size / sizeof( guint8 );
      for( i = 0; i < n_points; i++ )
        {
        guint8 raw_re = *( (guint8*)raw + n_points );
        data[i].re = (gfloat)raw_re / G_MAXUINT8;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_INT16:
      n_points = raw_size / sizeof( gint16 );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / G_MAXINT16;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_UINT16:
      n_points = raw_size / sizeof( guint16 );
      for( i = 0; i < n_points; i++ )
        {
        guint16 raw_re = GUINT16_FROM_LE( *( (guint16*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / G_MAXUINT16;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_INT32:
      n_points = raw_size / sizeof( gint32 );
      for( i = 0; i < n_points; i++ )
        {
        gint32 raw_re = GINT32_FROM_LE( *( (gint32*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / G_MAXINT32;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_UINT32:
      n_points = raw_size / sizeof( guint32 );
      for( i = 0; i < n_points; i++ )
        {
        guint32 raw_re = GUINT32_FROM_LE( *( (guint32*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / G_MAXUINT32;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_INT64:
      n_points = raw_size / sizeof( gint64 );
      for( i = 0; i < n_points; i++ )
        {
        gint64 raw_re = GINT64_FROM_LE( *( (gint64*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / G_MAXINT64;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_UINT64:
      n_points = raw_size / sizeof( guint64 );
      for( i = 0; i < n_points; i++ )
        {
        guint64 raw_re = GUINT64_FROM_LE( *( (guint64*)raw + n_points ) );
        data[i].re = (gfloat)raw_re / G_MAXUINT64;
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_INT8:
      n_points = raw_size / ( 2 * sizeof( gint8 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint8 raw_re = *( (gint8*)raw + 2 * n_points );
        gint8 raw_im = *( (gint8*)raw + 2 * n_points + 1 );
        data[i].re = (gfloat)raw_re / G_MAXINT8;
        data[i].im = (gfloat)raw_im / G_MAXINT8;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_UINT8:
      n_points = raw_size / ( 2 * sizeof( guint8 ) );
      for( i = 0; i < n_points; i++ )
        {
        guint8 raw_re = *( (guint8*)raw + 2 * n_points );
        guint8 raw_im = *( (guint8*)raw + 2 * n_points + 1 );
        data[i].re = (gfloat)raw_re / G_MAXUINT8;
        data[i].im = (gfloat)raw_im / G_MAXUINT8;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_INT16:
      n_points = raw_size / ( 2 * sizeof( gint16 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint16 raw_re = GINT16_FROM_LE( *( (gint16*)raw + 2 * n_points ) );
        gint16 raw_im = GINT16_FROM_LE( *( (gint16*)raw + 2 * n_points + 1 ) );
        data[i].re = (gfloat)raw_re / G_MAXINT16;
        data[i].im = (gfloat)raw_im / G_MAXINT16;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_UINT16:
      n_points = raw_size / ( 2 * sizeof( guint16 ) );
      for( i = 0; i < n_points; i++ )
        {
        guint16 raw_re = GUINT16_FROM_LE( *( (guint16*)raw + 2 * n_points ) );
        guint16 raw_im = GUINT16_FROM_LE( *( (guint16*)raw + 2 * n_points + 1 ) );
        data[i].re = (gfloat)raw_re / G_MAXUINT16;
        data[i].im = (gfloat)raw_im / G_MAXUINT16;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_INT32:
      n_points = raw_size / ( 2 * sizeof( gint32 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint32 raw_re = GINT32_FROM_LE( *( (gint32*)raw + 2 * n_points ) );
        gint32 raw_im = GINT32_FROM_LE( *( (gint32*)raw + 2 * n_points + 1 ) );
        data[i].re = (gfloat)raw_re / G_MAXINT32;
        data[i].im = (gfloat)raw_im / G_MAXINT32;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_UINT32:
      n_points = raw_size / ( 2 * sizeof( guint32 ) );
      for( i = 0; i < n_points; i++ )
        {
        guint32 raw_re = GUINT32_FROM_LE( *( (guint32*)raw + 2 * n_points ) );
        guint32 raw_im = GUINT32_FROM_LE( *( (guint32*)raw + 2 * n_points + 1 ) );
        data[i].re = (gfloat)raw_re / G_MAXUINT32;
        data[i].im = (gfloat)raw_im / G_MAXUINT32;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_INT64:
      n_points = raw_size / ( 2 * sizeof( gint64 ) );
      for( i = 0; i < n_points; i++ )
        {
        gint64 raw_re = GINT64_FROM_LE( *( (gint64*)raw + 2 * n_points ) );
        gint64 raw_im = GINT64_FROM_LE( *( (gint64*)raw + 2 * n_points + 1 ) );
        data[i].re = (gfloat)raw_re / G_MAXINT64;
        data[i].im = (gfloat)raw_im / G_MAXINT64;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_UINT64:
      n_points = raw_size / ( 2 * sizeof( guint64 ) );
      for( i = 0; i < n_points; i++ )
        {
        guint64 raw_re = GUINT64_FROM_LE( *( (guint64*)raw + 2 * n_points ) );
        guint64 raw_im = GUINT64_FROM_LE( *( (guint64*)raw + 2 * n_points + 1 ) );
        data[i].re = (gfloat)raw_re / G_MAXUINT64;
        data[i].im = (gfloat)raw_im / G_MAXUINT64;
        }
      break;

    case HYSCAN_DATA_TYPE_FLOAT:
      n_points = raw_size / sizeof( gfloat );
      for( i = 0; i < n_points; i++ )
        {
        data[i].re = *( (gfloat*)raw + n_points );
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_DOUBLE:
      n_points = raw_size / sizeof( gdouble );
      for( i = 0; i < n_points; i++ )
        {
        data[i].re = *( (gdouble*)raw + n_points );
        data[i].im = 0.0;
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_FLOAT:
      n_points = raw_size / ( 2 * sizeof( gfloat ) );
      for( i = 0; i < n_points; i++ )
        {
        data[i].re = *( (gfloat*)raw + 2 * n_points );
        data[i].im = *( (gfloat*)raw + 2 * n_points + 1 );
        }
      break;

    case HYSCAN_DATA_TYPE_COMPLEX_DOUBLE:
      n_points = raw_size / ( 2 * sizeof( gdouble ) );
      for( i = 0; i < n_points; i++ )
        {
        data[i].re = *( (gdouble*)raw + 2 * n_points );
        data[i].im = *( (gdouble*)raw + 2 * n_points + 1 );
        }
      break;

    default: return -1;

    }

  return n_points;

}

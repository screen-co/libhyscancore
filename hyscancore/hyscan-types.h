/*!
 * \file hyscan-types.h
 *
 * \brief Заголовочный файл вспомогательных функций с определениями типов объектов HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTypes HyScanTypes - вспомогательные функции с определениями типов объектов HyScan
 *
 * Группа функции HyScanTypes используется для определения следующих типов:
 *
 * - \link HyScanSonarType \endlink - типы гидролокаторов;
 * - \link HyScanBoardType \endlink - типы бортов;
 * - \link HyScanSignalType \endlink - типы сигналов;
 * - \link HyScanDataType \endlink - типы данных.
 *
 * Для сигналов доступны функции определения типа по имени сигнала и наоборот:
 *
 * - #hyscan_get_signal_type_by_name - функция определяет тип сигнала по его имени;
 * - #hyscan_get_signal_type_name - функция возвращает название сигнала для указанного типа.
 *
 * Для данных доступны функции определения типа по имени данных и наоборот, определения размера одного отсчёта
 * в данных и преобразования данных:
 *
 * - #hyscan_get_data_type_by_name - функция определяет тип данных по имени;
 * - #hyscan_get_data_type_name - функция возвращает название данных для указанного типа;
 * - #hyscan_get_data_point_size - функция возвращает размер одного отсчёта данных в байтах;
 * - #hyscan_import_data - функция преобразовывает данные из низкоуровневого формата в HyScanComplexFloat.
 *
*/

#ifndef _hyscan_types_h
#define _hyscan_types_h

#include <glib.h>

G_BEGIN_DECLS


/*! \brief Типы гидролокаторов */
typedef enum HyScanSonarType {

  HYSCAN_SONAR_UNKNOWN               = 0,        /*!< Неизвестный тип гидролокатора. */

  HYSCAN_SONAR_ECHO                  = 1,        /*!< Эхолот. */

  HYSCAN_SONAR_SIDE_SCAN             = 11,       /*!< Гидролокатор бокового обзора. */
  HYSCAN_SONAR_SIDE_SCAN_LF          = 11,       /*!< Гидролокатор бокового обзора - "низкая" частота. */
  HYSCAN_SONAR_SIDE_SCAN_HF          = 12,       /*!< Гидролокатор бокового обзора - "высокая" частота. */

  HYSCAN_SONAR_MULTI_BEAM            = 21,       /*!< Многолучевой эхолот. */

  HYSCAN_SONAR_PROFILE               = 31,       /*!< Профилограф. */

} HyScanSonarType;


/*! \brief Типы бортов */
typedef enum HyScanBoardType {

  HYSCAN_BOARD_UNKNOWN               = 0,        /*!< Неизвестный борт. */

  HYSCAN_BOARD_BOTTOM                = 1,        /*!< Под собой. */
  HYSCAN_BOARD_LEFT                  = 2,        /*!< Левый борт. */
  HYSCAN_BOARD_RIGHT                 = 3,        /*!< Правый борт. */
  HYSCAN_BOARD_BOW                   = 4,        /*!< Нос. */
  HYSCAN_BOARD_STERN                 = 5         /*!< Корма */

} HyScanBoardType;


/*! \brief Типы сигналов */
typedef enum HyScanSignalType {

  HYSCAN_SIGNAL_TYPE_UNKNOWN         = 0,        /*!< Неизвестный тип сигнала. */

  HYSCAN_SIGNAL_TYPE_TONE            = 1,        /*!< Тональный сигнал. */
  HYSCAN_SIGNAL_TYPE_LFM             = 2,        /*!< Линейно-частотно модулированный сигнал с увеличением частоты. */
  HYSCAN_SIGNAL_TYPE_LFMD            = 3,        /*!< Линейно-частотно модулированный сигнал с уменьшением частоты. */

} HyScanSignalType;


/*! \brief Типы данных */
typedef enum HyScanDataType {

  HYSCAN_DATA_TYPE_UNKNOWN           = 0,        /*!< Неизвестный тип данных. */

  HYSCAN_DATA_TYPE_ADC_12BIT         = 1,        /*!< Действительные отсчёты АЦП 12 бит. */
  HYSCAN_DATA_TYPE_ADC_14BIT         = 2,        /*!< Действительные отсчёты АЦП 14 бит. */
  HYSCAN_DATA_TYPE_ADC_16BIT         = 3,        /*!< Действительные отсчёты АЦП 16 бит. */

  HYSCAN_DATA_TYPE_COMPLEX_ADC_12BIT = 101,      /*!< Комплексные отсчёты АЦП 12 бит. */
  HYSCAN_DATA_TYPE_COMPLEX_ADC_14BIT = 102,      /*!< Комплексные отсчёты АЦП 14 бит. */
  HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT = 103,      /*!< Комплексные отсчёты АЦП 16 бит. */

  HYSCAN_DATA_TYPE_INT8              = 201,      /*!< Действительные целые 8 битные значения со знаком. */
  HYSCAN_DATA_TYPE_UINT8             = 202,      /*!< Действительные целые 8 битные значения без знака. */
  HYSCAN_DATA_TYPE_INT16             = 203,      /*!< Действительные целые 16 битные значения со знаком. */
  HYSCAN_DATA_TYPE_UINT16            = 204,      /*!< Действительные целые 16 битные значения без знака. */
  HYSCAN_DATA_TYPE_INT32             = 205,      /*!< Действительные целые 32 битные значения со знаком. */
  HYSCAN_DATA_TYPE_UINT32            = 206,      /*!< Действительные целые 32 битные значения без знака. */
  HYSCAN_DATA_TYPE_INT64             = 207,      /*!< Действительные целые 64 битные значения со знаком. */
  HYSCAN_DATA_TYPE_UINT64            = 208,      /*!< Действительные целые 64 битные значения без знака. */

  HYSCAN_DATA_TYPE_COMPLEX_INT8      = 301,      /*!< Комплексные целые 8 битные значения со знаком. */
  HYSCAN_DATA_TYPE_COMPLEX_UINT8     = 302,      /*!< Комплексные целые 8 битные значения без знака. */
  HYSCAN_DATA_TYPE_COMPLEX_INT16     = 303,      /*!< Комплексные целые 16 битные значения со знаком. */
  HYSCAN_DATA_TYPE_COMPLEX_UINT16    = 304,      /*!< Комплексные целые 16 битные значения без знака. */
  HYSCAN_DATA_TYPE_COMPLEX_INT32     = 305,      /*!< Комплексные целые 32 битные значения со знаком. */
  HYSCAN_DATA_TYPE_COMPLEX_UINT32    = 306,      /*!< Комплексные целые 32 битные значения без знака. */
  HYSCAN_DATA_TYPE_COMPLEX_INT64     = 307,      /*!< Комплексные целые 64 битные значения со знаком. */
  HYSCAN_DATA_TYPE_COMPLEX_UINT64    = 308,      /*!< Комплексные целые 64 битные значения без знака. */

  HYSCAN_DATA_TYPE_FLOAT             = 401,      /*!< Действительные float значения. */
  HYSCAN_DATA_TYPE_DOUBLE            = 402,      /*!< Действительные double значения. */

  HYSCAN_DATA_TYPE_COMPLEX_FLOAT     = 501,      /*!< Комплексные float значения. */
  HYSCAN_DATA_TYPE_COMPLEX_DOUBLE    = 502       /*!< Комплексные double значения. */

} HyScanDataType;


typedef struct HyScanComplexFloat {              // Комплексные числа.

  gfloat                     re;                 // Действительная часть.
  gfloat                     im;                 // Мнимая часть.

} HyScanComplexFloat;


/*!
 *
 * Функция преобразовывает строку с названием типа сигнала в нумерованное значение.
 *
 * \param signal_name название типа сигнала.
 *
 * \return Тип сигнала \link HyScanSignalType \endlink.
 *
*/
HyScanSignalType hyscan_get_signal_type_by_name( const gchar *signal_name );


/*!
 *
 * Функция преобразовывает нумерованное значение типа данных в название типа.
 *
 * Функция возвращает строку статически размещённую в памяти, эта строка не должна модифицироваться.
 *
 * \param signal_type тип сигнала.
 *
 * \return Строка с названием типа сигнала.
 *
*/
const gchar *hyscan_get_signal_type_name( HyScanSignalType signal_type );


/*!
 *
 *  Функция преобразовывает строку с названием типа данных в нумерованное значение.
 *
 * \param data_name название типа данных.
 *
 * \return Тип данных \link HyScanDataType \endlink.
 *
*/
HyScanDataType hyscan_get_data_type_by_name( const gchar *data_name );


/*!
 *
 *  Функция преобразовывает нумерованное значение типа данных в название типа.
 *
 * Функция возвращает строку статически размещённую в памяти, эта строка не должна модифицироваться.
 *
 * \param data_type тип данных.
 *
 * \return Строка с названием типа данных.
 *
*/
const gchar *hyscan_get_data_type_name( HyScanDataType data_type );


/*!
 *
 *  Функция возвращает размер одного отсчёта данных в байтах, для указанного типа.
 *
 * \param data_type тип данных.
 *
 * \return Размер одного отсчёта данных в байтах.
 *
*/
gint32 hyscan_get_data_point_size( HyScanDataType data_type );


/*!
 *
 *  Функция преобразовывает данные из низкоуровневого формата в HyScanComplexFloat.
 *
 * Для данных, хранящихся в целочисленных форматах, используется преобразование из little endian в формат
 * хранения данных используемого процессора.
 *
 * \param data_type тип данных;
 * \param data указатель на преобразовываемые данные;
 * \param data_size размер преобразовываемых данных;
 * \param buffer указатель на буфер для преобразованных данных;
 * \param buffer_size размер буфера для преобразованных данных в точках.
 *
 * \return TRUE - если преобразование выполнено, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_import_data( HyScanDataType data_type, gpointer data, gint32 data_size, HyScanComplexFloat *buffer, gint32 *buffer_size );


G_END_DECLS

#endif // _hyscan_types_h

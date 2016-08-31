/**
 * \file hyscan-core-types.h
 *
 * \brief Заголовочный файл вспомогательных функций и определений типов объектов используемых HyScanCore
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCoreTypes HyScanCoreTypes - вспомогательные функции и определения типов объектов используемых HyScanCore
 *
 * В HyScanCoreTypes вводятся определения следующих типов:
 *
 * - \link HyScanSonarSourceType \endlink - источники данных;
 * - \link HyScanTrackType \endlink - типы галсов.
 *
 * Все источники данных разделены на два основных вида:
 *
 * - датчики
 * - источники гидролокационных данных.
 *
 * Функции #hyscan_track_get_name_by_type и #hyscan_track_get_type_by_name используются для
 * преобразования типа галсов в строковое представление и наоборот.
 *
 * Источники гидролокационных данных разделяются на следующие виды:
 *
 * - источники "сырых" гидролокационных данных;
 * - источники акустических данных;
 * - источники батиметрических данных.
 *
 * Источники "сырых" гидролокационных данных содержат информацию об амплитуде эхо сигнала
 * дискретизированной во времени. Это вид первичной информации, получаемой от приёмников
 * гидролокатора.
 *
 * Источники акустических данных содержат обработанную информацию от гидролокаторов
 * бокового обзора, эхолота, профилографа и т.п.
 *
 * Источники батиметрических данных содержат обработанную информацию о глубине.
 *
 * Функции #hyscan_source_is_sensor, #hyscan_source_is_raw и #hyscan_source_is_acoustic
 * могут использоваться для проверки принадлежности источника данных к определённому типу.
 *
 * В HyScanCoreTypes вводятся определения следующих структур:
 *
 * - \link HyScanAntennaPosition \endlink - параметры местоположения приёмной антенны;
 * - \link HyScanRawDataInfo \endlink - параметры "сырых" гидролокационных данных.
 * - \link HyScanAcousticDataInfo \endlink - параметры акустических данных.
 *
 * В структуре \link HyScanAntennaPosition \endlink присутствует информация о местоположении
 * приёмных антенн. Смещения приёмной антенны указываются относительно центра масс судна.
 * При этом ось X направлена в нос, ось Y на правый борт, ось Z вверх.
 *
 * Углы установки антенны указываются для вектора, перпиндикулярного её рабочей
 * плоскости. Угол psi учитывает разворот антенны по курсу от её нормального положения.
 * Угол gamma учитывает разворот антенны по крену. Угол theta учитывает разворот
 * антенны по дифференту от её нормального положения.
 *
 * Положительные значения указываются для углов отсчитываемых против часовой стрелки.
 *
 * Местоположение приёмной антенны должно быть задано для всех источников данных.
 *
 * Для определения названий каналов, по типу сохраняемых в них данных и их характеристикам,
 * предназначена функция #hyscan_channel_get_name_by_types. Определить характеристики данных
 * по названию канала можно функцией #hyscan_channel_get_types_by_name.
 *
 */

#ifndef __HYSCAN_CORE_TYPES_H__
#define __HYSCAN_CORE_TYPES_H__

#include <hyscan-db.h>
#include <hyscan-data.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

/** \brief Типы источников данных */
typedef enum
{
  HYSCAN_SOURCE_INVALID                        = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD            = 101,          /**< Боковой обзор, правый борт. */
  HYSCAN_SOURCE_SIDE_SCAN_PORT                 = 102,          /**< Боковой обзор, левый борт. */
  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI         = 103,          /**< Боковой обзор, правый борт, высокое разрешение. */
  HYSCAN_SOURCE_SIDE_SCAN_PORT_HI              = 104,          /**< Боковой обзор, левый борт, высокое разрешение. */
  HYSCAN_SOURCE_INTERFEROMETRY_STARBOARD       = 105,          /**< Интерферометрия, правый борт. */
  HYSCAN_SOURCE_INTERFEROMETRY_PORT            = 106,          /**< Интерферометрия, левый борт. */
  HYSCAN_SOURCE_ECHOSOUNDER                    = 107,          /**< Эхолот. */
  HYSCAN_SOURCE_PROFILER                       = 108,          /**< Профилограф. */
  HYSCAN_SOURCE_LOOK_AROUND_STARBOARD          = 109,          /**< Круговой обзор, правый борт. */
  HYSCAN_SOURCE_LOOK_AROUND_PORT               = 110,          /**< Круговой обзор, левый борт. */
  HYSCAN_SOURCE_FORWARD_LOOK                   = 111,          /**< Вперёдсмотрящий гидролокатор. */

  HYSCAN_SOURCE_SAS                            = 201,          /**< Сообщения САД. */
  HYSCAN_SOURCE_SAS_V2                         = 202,          /**< Сообщения САД, версия 2. */

  HYSCAN_SOURCE_NMEA_ANY                       = 301,          /**< Любые сообщения NMEA. */
  HYSCAN_SOURCE_NMEA_GGA                       = 302,          /**< Сообщения NMEA GGA. */
  HYSCAN_SOURCE_NMEA_RMC                       = 303,          /**< Сообщения NMEA RMC. */
  HYSCAN_SOURCE_NMEA_DPT                       = 304           /**< Сообщения NMEA DPT. */
} HyScanSourceType;

/** \brief Типы галсов */
typedef enum
{
  HYSCAN_TRACK_UNSPECIFIED                     = 0,            /**< Неопределённый тип. */

  HYSCAN_TRACK_SURVEY                          = 101,          /**< Галс с данными съёмки. */
  HYSCAN_TRACK_TACK                            = 102,          /**< Лавировочный галс. */
  HYSCAN_TRACK_TRACK                           = 103           /**< Треки движения судна. */
} HyScanTrackType;

/** \brief Параметры местоположения приёмной антенны */
typedef struct
{
  gdouble                      x;                              /**< Смещение антенны по оси X, метры. */
  gdouble                      y;                              /**< Смещение антенны по оси Y, метры. */
  gdouble                      z;                              /**< Смещение антенны по оси Z, метры. */

  gdouble                      psi;                            /**< Поворот антенны по курсу, радианы. */
  gdouble                      gamma;                          /**< Поворот антенны по крену, радианы. */
  gdouble                      theta;                          /**< Поворот антенны по дифференту, радианы. */
} HyScanAntennaPosition;

/** \brief Параметры "сырых" гидролокационных данных */
typedef struct
{
  struct                                                       /**< Параметры данных. */
  {
    HyScanDataType             type;                           /**< Тип данных. */
    gdouble                    rate;                           /**< Частота дискретизации, Гц. */
  } data;

  struct                                                       /**< Параметры антенны. */
  {
    struct
    {
      gdouble                  vertical;                       /**< Смещение антенны в "решётке" в вертикальной плоскости, м. */
      gdouble                  horizontal;                     /**< Смещение антенны в "решётке" в горизонтальной плоскости, м. */
    } offset;
    struct
    {
      gdouble                  vertical;                       /**< Диаграмма направленности в вертикальной плоскости, рад. */
      gdouble                  horizontal;                     /**< Диаграмма направленности в горизонтальной плоскости, рад. */
    } pattern;
  } antenna;

  struct                                                       /**< Параметры АЦП. */
  {
    gdouble                    vref;                           /**< Опорное напряжение, В. */
    gint                       offset;                         /**< Смещение нуля, отсчёты. */
  } adc;
} HyScanRawDataInfo;

/** \brief Параметры акустических данных */
typedef struct
{
  struct                                                       /**< Параметры данных. */
  {
    HyScanDataType             type;                           /**< Тип данных. */
    gdouble                    rate;                           /**< Частота дискретизации, Гц. */
  } data;

  struct                                                       /**< Параметры антенны. */
  {
    struct
    {
      gdouble                  vertical;                       /**< Диаграмма направленности в вертикальной плоскости, рад. */
      gdouble                  horizontal;                     /**< Диаграмма направленности в горизонтальной плоскости, рад. */
    } pattern;
  } antenna;
} HyScanAcousticDataInfo;

/**
 *
 * Функция проверяет тип источника данных на соответствие одному из типов датчиков.
 *
 * \param source тип источника данных.
 *
 * \return TRUE - если тип относится к данным датчиков, иначе - FALSE;
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_source_is_sensor                 (HyScanSourceType               source);

/**
 *
 * Функция проверяет тип источника данных на соответствие "сырым" гидролокационным данным.
 *
 * \param source тип источника данных.
 *
 * \return TRUE - если тип относится к "сырым" гидролокационным данным, иначе - FALSE;
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_source_is_raw                    (HyScanSourceType               source);

/**
 *
 * Функция проверяет тип источника данных на соответствие акустическим данным.
 *
 * \param source тип источника данных.
 *
 * \return TRUE - если тип относится к акустическим данным, иначе - FALSE;
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_source_is_acoustic               (HyScanSourceType               source);

/**
 *
 * Функция преобразовывает численное значени типа галса в название типа.
 *
 * \param type тип галса.
 *
 * \return Строка с названием типа галса.
 *
 */
HYSCAN_CORE_EXPORT
const gchar           *hyscan_track_get_name_by_type           (HyScanTrackType                type);

/**
 *
 * Функция преобразовывает строку с названием типа галса в численное значение.
 *
 * \param name название типа галса.
 *
 * \return Тип галса \link HyScanTrackType \endlink.
 *
 */
HYSCAN_CORE_EXPORT
HyScanTrackType        hyscan_track_get_type_by_name           (const gchar                   *name);

/**
 *
 * Функция возвращает название канала для указанных характеристик.
 * Строка, возвращаемая этой функцией, не должна изменяться пользователем.
 *
 * \param source тип источника данных;
 * \param raw признак "сырых" данных;
 * \param channel индекс канала данных, начиная с 1.
 *
 * \return Строка с названием канала или NULL - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
const gchar           *hyscan_channel_get_name_by_types        (HyScanSourceType               source,
                                                                gboolean                       raw,
                                                                guint                          channel);

/**
 *
 * Функция возвращает характеристики канала данных по его имени.
 *
 * \param name название канала данных;
 * \param source переменная для типа источника данных или NULL;
 * \param raw переменная для признака "сырых" данных или NULL;
 * \param channel переменная для индекса канала данных или NULL.
 *
 * \return TRUE - если характеристики канала определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_channel_get_types_by_name        (const gchar                   *name,
                                                                HyScanSourceType              *source,
                                                                gboolean                      *raw,
                                                                guint                         *channel);

G_END_DECLS

#endif /* __HYSCAN_TYPES_H__ */

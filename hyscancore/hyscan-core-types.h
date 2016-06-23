/**
 * \file hyscan-core-types.h
 *
 * \brief Заголовочный файл вспомогательных функций и определения типов объектов используемых HyScanCore
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCoreTypes HyScanCoreTypes - вспомогательные функции и определения типов объектов используемых HyScanCore
 *
 * В HyScanCoreTypes вводятся определения следующих типов:
 *
 * - \link HyScanSonarSourceType \endlink - источники данных;
 * - \link HyScanTrackType \endlink - типы галсаов;
 * - \link HyScanCardinalDirectionType \endlink - стороны света.
 *
 * В HyScanCoreTypes вводятся определения следующих структур:
 *
 * - \link HyScanSensorChannelInfo \endlink - параметры канала с данными датчиков;
 * - \link HyScanDataChannelInfo \endlink - параметры канала с акустическими данными.
 *
 * В этих структурах присутствует информация о местоположении приёмных антенн.
 * Смещения приёмной антенны указываются относительно центра масс судна. При этом
 * ось X направлена в нос, ось Y на правый борт, ось Z вверх.
 *
 * Углы установки антенны указываются для вектора, перпиндикулярного её рабочей
 * плоскости. Угол psi учитывает разворот антенны по курсу от её нормального положения.
 * Угол gamma учитывает разворот антенны по крену. Угол theta учитывает разворот
 * антенны по дифференту от её нормального положения.
 *
 * Положительные значения указываются для углов отсчитываемых против часовой стрелки.
 *
 * Для определения названий каналов, по типу сохраняемых в них данных и их характеристикам,
 * предназначена функция #hyscan_channel_get_name_by_types. Определить характеристики данных
 * по названию канала можно функцией #hyscan_channel_get_types_by_name.
 *
 * Для создания галса в проекте необходимо использовать функцию #hyscan_track_create. При использовании
 * этой функции будет определена схема данных галса и его каналов, используемая библиотекой HyScanCore.
 *
 * Для создания канала для записи данных от датчиков необходимо использовать
 * функцию #hyscan_channel_sensor_create.
 *
 */

#ifndef __HYSCAN_CORE_TYPES_H__
#define __HYSCAN_CORE_TYPES_H__

#include <hyscan-db.h>
#include <hyscan-data.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

/** \brief Типы источников гидролокационных данных и данных датчиков */
typedef enum
{
  HYSCAN_SOURCE_INVALID                             = 0,       /**< Недопустимый тип, ошибка. */

  HYSCAN_SOURCE_ECHOSOUNDER                         = 101,     /**< Эхолот. */
  HYSCAN_SOURCE_SS_STARBOARD                        = 102,     /**< Правый борт, боковой обзор. */
  HYSCAN_SOURCE_SS_PORT                             = 103,     /**< Левый борт, боковой обзор. */
  HYSCAN_SOURCE_SS_STARBOARD_HI                     = 104,     /**< Правый борт, боковой обзор, высокое разрешение. */
  HYSCAN_SOURCE_SS_PORT_HI                          = 105,     /**< Левый борт, боковой обзор, высокое разрешение. */
  HYSCAN_SOURCE_ISS_STARBOARD                       = 106,     /**< Правый борт, интерферометр. */
  HYSCAN_SOURCE_ISS_PORT                            = 107,     /**< Левый борт, интерферометр. */
  HYSCAN_SOURCE_LA_STARBOARD                        = 108,     /**< Правый борт, круговой обзор. */
  HYSCAN_SOURCE_LA_PORT                             = 109,     /**< Левый борт, круговой обзор. */
  HYSCAN_SOURCE_PROFILER                            = 110,     /**< Профилограф. */
  HYSCAN_SOURCE_FL                                  = 111,     /**< Вперёдсмотрящий гидролокатор. */

  HYSCAN_SOURCE_SAS                                 = 201,     /**< Сообщения САД. */

  HYSCAN_SOURCE_NMEA_ANY                            = 301,     /**< Любые сообщения NMEA. */
  HYSCAN_SOURCE_NMEA_GGA                            = 302,     /**< Сообщения NMEA GGA. */
  HYSCAN_SOURCE_NMEA_RMC                            = 303,     /**< Сообщения NMEA RMC. */
  HYSCAN_SOURCE_NMEA_DPT                            = 304      /**< Сообщения NMEA DPT. */
} HyScanSourceType;

/** \brief Типы галсов */
typedef enum
{
  HYSCAN_TRACK_UNSPECIFIED                          = 0,       /**< Неопределённый тип. */

  HYSCAN_TRACK_SURVEY                               = 101,     /**< Галс с данными съёмки. */
  HYSCAN_TRACK_TACK                                 = 102,     /**< Лавировочный галс. */
  HYSCAN_TRACK_TRACK                                = 103      /**< Треки движения судна. */
} HyScanTrackType;

/** \brief Стороны света */
typedef enum
{
  HYSCAN_CARDINAL_DIRECTION_INVALID                 = 0,       /**< Недопустимая сторона света, ошибка. */

  HYSCAN_CARDINAL_DIRECTION_NORTH                   = 101,     /**< Север. */
  HYSCAN_CARDINAL_DIRECTION_EAST                    = 102,     /**< Восток. */
  HYSCAN_CARDINAL_DIRECTION_SOUTH                   = 103,     /**< Юг. */
  HYSCAN_CARDINAL_DIRECTION_WEST                    = 104      /**< Запад. */
} HyScanCardinalDirectionType;

/** \brief Параметры канала с данными датчиков */
typedef struct
{
  gdouble                      x;                              /**< Смещение антенны по оси X, метры. */
  gdouble                      y;                              /**< Смещение антенны по оси Y, метры. */
  gdouble                      z;                              /**< Смещение антенны по оси Z, метры. */

  gdouble                      psi;                            /**< Поворот антенны по курсу, радианы. */
  gdouble                      gamma;                          /**< Поворот антенны по крену, радианы. */
  gdouble                      theta;                          /**< Поворот антенны по дифференту, радианы. */
} HyScanSensorChannelInfo;

/** \brief Параметры канала с акустическими данными */
typedef struct
{
  HyScanDataType               discretization_type;            /**< Тип дискретизации данных. */
  gdouble                      discretization_frequency;       /**< Частота дискретизации данных. */

  gdouble                      vertical_pattern;               /**< Диаграмма направленности антенны в вертикальной плоскости. */
  gdouble                      horizontal_pattern;             /**< Диаграмма направленности антенны в горизонтальной плоскости. */

  gdouble                      x;                              /**< Смещение антенны по оси X, метры. */
  gdouble                      y;                              /**< Смещение антенны по оси Y, метры. */
  gdouble                      z;                              /**< Смещение антенны по оси Z, метры. */

  gdouble                      psi;                            /**< Угол поворота антенны по курсу, радианы. */
  gdouble                      gamma;                          /**< Угол поворота антенны по крену, радианы. */
  gdouble                      theta;                          /**< Угол поворота антенны по дифференту, радианы. */
} HyScanDataChannelInfo;

/**
 *
 * Функция возвращает название канала для указанных характеристик.
 * Строка, возвращаемая этой функцией, не должна изменяться пользователем.
 *
 * \param src источник данных;
 * \param raw признак "сырых" данных;
 * \param index индекс канала данных, начиная с 1.
 *
 * \return Строка с названием канала или NULL - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
const gchar           *hyscan_channel_get_name_by_types        (HyScanSourceType               src,
                                                                gboolean                       raw,
                                                                gint                           index);

/**
 *
 * Функция возвращает характеристики канала данных по его имени.
 *
 * \param name название канала данных;
 * \param src переменная для источника данных или NULL;
 * \param raw переменная для признака "сырых" данных или NULL;
 * \param index переменная для индекса канала данных или NULL.
 *
 * \return TRUE - если характеристики канала определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_channel_get_types_by_name        (const gchar                   *name,
                                                                HyScanSourceType              *src,
                                                                gboolean                      *raw,
                                                                gint                          *index);

/**
 *
 * Функция создаёт галс в системе хранения. При создании галса его тип.
 * Проект должен быть создан заранее.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param track_type тип галса.
 *
 * \return TRUE - если галс был создан, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_track_create                     (HyScanDB                      *db,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name,
                                                                HyScanTrackType                track_type);

/**
 *
 * Функция создаёт канал для записи данных от датчиков. Проект и галс
 * должны быть созданы заранее.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 * \param channel_info параметры канала данных.
 *
 * \return Идентификатор канала данных или отрицательное число в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gint32                 hyscan_channel_sensor_create            (HyScanDB                      *db,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name,
                                                                const gchar                   *channel_name,
                                                                HyScanSensorChannelInfo       *channel_info);

G_END_DECLS

#endif /* __HYSCAN_TYPES_H__ */

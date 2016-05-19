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
 * - \link HyScanSonarType \endlink - типы гидролокаторов;
 * - \link HyScanTrackType \endlink - типы галсаов;
 * - \link HyScanSonarDataType \endlink - типы гидролокационных данных;
 * - \link HyScanSonarChannelIndex \endlink - номера каналов данных;
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

/** \brief Типы гидролокаторов */
typedef enum
{
  HYSCAN_SONAR_INVALID,                                        /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_ECHOSOUNDER,                                    /**< Эхолот. */
  HYSCAN_SONAR_SIDE_SCAN,                                      /**< Гидролокатор бокового обзора. */
  HYSCAN_SONAR_SIDE_SCAN_DF,                                   /**< Гидролокатор бокового обзора двухчастотный. */
  HYSCAN_SONAR_LOOK_AROUND,                                    /**< Гидролокатор кругового обзора. */
  HYSCAN_SONAR_LOOK_AHEAD,                                     /**< Вперёд смотрящий гидролокатор. */
  HYSCAN_SONAR_PROFILER,                                       /**< Профилограф. */
  HYSCAN_SONAR_MULTI_BEAM,                                     /**< Многолучевой эхолот. */
  HYSCAN_SONAR_INTERFEROMETER,                                 /**< Интерферометрический гидролокатор. */
} HyScanSonarType;

/** \brief Типы галсов. */
typedef enum
{
  HYSCAN_TRACK_UNSPECIFIED,                                    /**< Неопределённый тип. */

  HYSCAN_TRACK_SURVEY,                                         /**< Галс с данными съёмки. */
  HYSCAN_TRACK_TACK,                                           /**< Лавировочный галс. */
  HYSCAN_TRACK_TRACK                                           /**< Треки движения судна. */
} HyScanTrackType;

/** \brief Типы гидролокационных данных */
typedef enum
{
  HYSCAN_SONAR_DATA_INVALID,                                   /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_DATA_ECHOSOUNDER,                               /**< Эхолот. */
  HYSCAN_SONAR_DATA_SS_STARBOARD,                              /**< Боковой обзор, правый борт. */
  HYSCAN_SONAR_DATA_SS_PORT,                                   /**< Боковой обзор, левый борт. */

  HYSCAN_SONAR_DATA_SAS,                                       /**< Сообщения САД. */

  HYSCAN_SONAR_DATA_NMEA_ANY,                                  /**< Любые сообщения NMEA. */
  HYSCAN_SONAR_DATA_NMEA_GGA,                                  /**< Сообщения NMEA GGA. */
  HYSCAN_SONAR_DATA_NMEA_RMC,                                  /**< Сообщения NMEA RMC. */
  HYSCAN_SONAR_DATA_NMEA_DPT,                                  /**< Сообщения NMEA DPT. */
} HyScanSonarDataType;

/** \brief Номера каналов */
typedef enum
{
  HYSCAN_SONAR_CHANNEL_INVALID,                                /**< Недопустимый номер, ошибка. */

  HYSCAN_SONAR_CHANNEL_1,                                      /**< Канал 1. */
  HYSCAN_SONAR_CHANNEL_2,                                      /**< Канал 2. */
  HYSCAN_SONAR_CHANNEL_3,                                      /**< Канал 3. */
  HYSCAN_SONAR_CHANNEL_4,                                      /**< Канал 4. */
  HYSCAN_SONAR_CHANNEL_5,                                      /**< Канал 5. */
  HYSCAN_SONAR_CHANNEL_6,                                      /**< Канал 6. */
  HYSCAN_SONAR_CHANNEL_7,                                      /**< Канал 7. */
  HYSCAN_SONAR_CHANNEL_8,                                      /**< Канал 8. */
} HyScanSonarChannelIndex;

/** \brief Стороны света */
typedef enum
{
  HYSCAN_CARDINAL_DIRECTION_INVALID,                           /**< Недопустимая сторона света, ошибка. */

  HYSCAN_CARDINAL_DIRECTION_NORTH,                             /**< Север. */
  HYSCAN_CARDINAL_DIRECTION_EAST,                              /**< Восток. */
  HYSCAN_CARDINAL_DIRECTION_SOUTH,                             /**< Юг. */
  HYSCAN_CARDINAL_DIRECTION_WEST,                              /**< Запад. */
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

  gdouble                      psi;                            /**< Поворот антенны по курсу, радианы. */
  gdouble                      gamma;                          /**< Поворот антенны по крену, радианы. */
  gdouble                      theta;                          /**< Поворот антенны по дифференту, радианы. */
} HyScanDataChannelInfo;

/**
 *
 * Функция возвращает название канала для указанных характеристик.
 * Строка, возвращаемая этой функцией, не должна изменяться пользователем.
 *
 * Индекс канала данных для одноканальных систем (ГБО, Эхолот) должен быть равным нулю.
 *
 * \param data_type тип данных;
 * \param hi_res признак повышенного разрешения;
 * \param raw признак "сырых" данных;
 * \param index индекс канала данных.
 *
 * \return Строка с названием канала или NULL - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
const gchar           *hyscan_channel_get_name_by_types        (HyScanSonarDataType            data_type,
                                                                gboolean                       hi_res,
                                                                gboolean                       raw,
                                                                HyScanSonarChannelIndex        index);

/**
 *
 * Функция возвращает характеристики канала данных по его имени.
 *
 * \param channel_name название канала данных;
 * \param data_type переменная для типа данных или NULL;
 * \param hi_res переменная для признака повышенного разрешения или NULL;
 * \param raw переменная для признака "сырых" данных или NULL;
 * \param index переменная для индекса канала данных или NULL.
 *
 * \return TRUE - если характеристики канала определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_channel_get_types_by_name        (const gchar                   *channel_name,
                                                                HyScanSonarDataType           *data_type,
                                                                gboolean                      *hi_res,
                                                                gboolean                      *raw,
                                                                HyScanSonarChannelIndex       *index);

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

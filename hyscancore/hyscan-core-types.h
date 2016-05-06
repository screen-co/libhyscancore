/**
 * \file hyscan-core-types.h
 *
 * \brief Заголовочный файл вспомогательных функций с определениями типов объектов используемых HyScanCore
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCoreTypes HyScanCoreTypes - вспомогательные функции с определениями типов объектов используемых HyScanCore
 *
 * В HyScanCoreTypes вводятся определения следующих типов:
 *
 * - \link HyScanSonarType \endlink - типы гидролокаторов;
 * - \link HyScanSonarDataType \endlink - типы гидролокационных данных;
 * - \link HyScanSonarChannel \endlink - номера каналов данных;
 * - \link HyScanCardinalDirectionType \endlink - стороны света.
 *
 * Для определения названий каналов, по типу сохраняемых в них данных и их характеристикам,
 * предназначена функция #hyscan_channel_get_name_by_types.  Определить характеристики данных
 * по названию канала можно функцией #hyscan_channel_get_types_by_name.
 *
 */

#ifndef __HYSCAN_CORE_TYPES_H__
#define __HYSCAN_CORE_TYPES_H__

#include <hyscan-data.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

/** \brief Типы гидролокаторов */
typedef enum
{
  HYSCAN_SONAR_INVALID,                                /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_ECHOSOUNDER,                            /**< Эхолот. */
  HYSCAN_SONAR_SIDE_SCAN,                              /**< Гидролокатор бокового обзора. */
  HYSCAN_SONAR_SIDE_SCAN_DF,                           /**< Гидролокатор бокового обзора двухчастотный. */
  HYSCAN_SONAR_LOOK_AROUND,                            /**< Гидролокатор кругового обзора. */
  HYSCAN_SONAR_LOOK_AHEAD,                             /**< Вперёд смотрящий гидролокатор. */
  HYSCAN_SONAR_PROFILER,                               /**< Профилограф. */
  HYSCAN_SONAR_MULTI_BEAM,                             /**< Многолучевой эхолот. */
  HYSCAN_SONAR_INTERFEROMETER,                         /**< Интерферометрический гидролокатор. */
} HyScanSonarType;

/** \brief Типы гидролокационных данных */
typedef enum
{
  HYSCAN_SONAR_DATA_INVALID,                           /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_DATA_ECHOSOUNDER,                       /**< Эхолот. */
  HYSCAN_SONAR_DATA_SS_STARBOARD,                      /**< Боковой обзор, правый борт. */
  HYSCAN_SONAR_DATA_SS_PORT,                           /**< Боковой обзор, левый борт. */

  HYSCAN_SONAR_DATA_SAS,                               /**< Сообщения САД. */

  HYSCAN_SONAR_DATA_NMEA_ANY,                          /**< Любые сообщения NMEA. */
  HYSCAN_SONAR_DATA_NMEA_GGA,                          /**< Сообщения NMEA GGA. */
  HYSCAN_SONAR_DATA_NMEA_RMC,                          /**< Сообщения NMEA RMC. */
  HYSCAN_SONAR_DATA_NMEA_DPT,                          /**< Сообщения NMEA DPT. */
} HyScanSonarDataType;

/** \brief Номера каналов */
typedef enum
{
  HYSCAN_SONAR_CHANNEL_INVALID,                        /**< Недопустимый номер, ошибка. */

  HYSCAN_SONAR_CHANNEL_1,                              /**< Канал 1. */
  HYSCAN_SONAR_CHANNEL_2,                              /**< Канал 2. */
  HYSCAN_SONAR_CHANNEL_3,                              /**< Канал 3. */
  HYSCAN_SONAR_CHANNEL_4,                              /**< Канал 4. */
  HYSCAN_SONAR_CHANNEL_5,                              /**< Канал 5. */
  HYSCAN_SONAR_CHANNEL_6,                              /**< Канал 6. */
  HYSCAN_SONAR_CHANNEL_7,                              /**< Канал 7. */
  HYSCAN_SONAR_CHANNEL_8,                              /**< Канал 8. */
} HyScanSonarChannel;

/** \brief Стороны света */
typedef enum
{
  HYSCAN_CARDINAL_DIRECTION_INVALID,                   /**< Недопустимая сторона света, ошибка. */

  HYSCAN_CARDINAL_DIRECTION_NORTH,                     /**< Север. */
  HYSCAN_CARDINAL_DIRECTION_EAST,                      /**< Восток. */
  HYSCAN_CARDINAL_DIRECTION_SOUTH,                     /**< Юг. */
  HYSCAN_CARDINAL_DIRECTION_WEST,                      /**< Запад. */
} HyScanCardinalDirectionType;

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
                                                                gint                           index);

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
                                                                gint                          *index);

G_END_DECLS

#endif /* __HYSCAN_TYPES_H__ */

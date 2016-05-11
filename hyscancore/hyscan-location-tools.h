/**
 * \file hyscan-location-tools.h
 *
 * \brief Заголовочный файл вспомогательных функций для класса HyScanLocation
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanLocationTools HyScanLocationTools - вспомогательные функции для класса HyScanLocation
 *
 * Эта библиотека функций предназначена исключительно для работы класса HyScanLocation. Отдельно её подключать нет смысла.
 * Содержит следующие функции.
 *
 * Для работы с сырыми данными:
 * - #hyscan_location_nmea_latlong_get - извлекает значение широты/долготы из NMEA-строки;
 * - #hyscan_location_nmea_altitude_get - извлекает значение высоты из NMEA-строки;
 * - #hyscan_location_nmea_track_get - извлекает значение курса из NMEA-строки;
 * - #hyscan_location_nmea_roll_get - извлекает значение крена из NMEA-строки;
 * - #hyscan_location_nmea_pitch_get - извлекает значение дифферента из NMEA-строки;
 * - #hyscan_location_nmea_speed_get - извлекает значение скорости из NMEA-строки;
 * - #hyscan_location_nmea_depth_get - извлекает значение глубины из NMEA-строки;
 * - #hyscan_location_nmea_datetime_get - извлекает значение даты и времени из NMEA-строки;
 * - #hyscan_location_nmea_time_get - извлекает значение времени из NMEA-строки;
 * - #hyscan_location_echosounder_depth_get - определяет глубину по эхолоту;
 * - #hyscan_location_sonar_depth_get - определяет глубину по ГБО.
 *
 * Для фильтрации данных:
 * - #hyscan_location_4_point_2d_bezier - сглаживание кривой Безье;
 * - #hyscan_location_3_point_2d_bezier - не используется;
 * - #hyscan_location_3_point_average - усреднение по трем точкам.
 * - #hyscan_location_shift - для сдвижки данных в пространстве с учетом курса, крена и дифферента.
 *
 * Функции надзирателя:
 * - #hyscan_location_overseer_latlong - следит за данными широты;
 * - #hyscan_location_overseer_altitude - следит за данными высоты;
 * - #hyscan_location_overseer_track - следит за данными курса;
 * - #hyscan_location_overseer_roll - следит за данными крена;
 * - #hyscan_location_overseer_pitch - следит за данными дифферента;
 * - #hyscan_location_overseer_speed - следит за данными скорости;
 * - #hyscan_location_overseer_depth - следит за данными глубины;
 * - #hyscan_location_overseer_datetime - следит за данными даты и времени.
 *
 * Функции-getter'ы:
 * - #hyscan_location_getter_latlong - аппроксимация и выдача данных широты;
 * - #hyscan_location_getter_altitude - аппроксимация и выдача данных высоты;
 * - #hyscan_location_getter_track - аппроксимация и выдача данных курса;
 * - #hyscan_location_getter_roll - аппроксимация и выдача данных крена;
 * - #hyscan_location_getter_pitch - аппроксимация и выдача данных дифферента;
 * - #hyscan_location_getter_speed - аппроксимация и выдача данных скорости;
 * - #hyscan_location_getter_depth - аппроксимация и выдача данных глубины;
 * - #hyscan_location_getter_datetime - аппроксимация и выдача данных даты;
 * - #hyscan_location_getter_gdouble2 - внутренняя функция для получения данных типа HyScanLocationGdouble2;
 * - #hyscan_location_getter_gdouble1 - внутренняя функция для получения данных типа HyScanLocationGdouble1.
 */
#ifndef __HYSCAN_LOCATION_TOOLS_H__
#define __HYSCAN_LOCATION_TOOLS_H__

#include <hyscan-location.h>
#include <hyscan-core-types.h>
#include <math.h>
#include <string.h>

G_BEGIN_DECLS;


/* Функции работы с NMEA-строками. */

/** Извлекает широту и долготу из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble2 со значениями. */
HyScanLocationGdouble2  hyscan_location_nmea_latlong_get        (gchar  *input);

/** Извлекает высоту из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble1 со значением. */
HyScanLocationGdouble1  hyscan_location_nmea_altitude_get       (gchar  *input);

/** Извлекает курс из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble1 со значением. */
HyScanLocationGdouble1  hyscan_location_nmea_track_get          (gchar  *input);

/** Извлекает крен из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble1 со значением. */
HyScanLocationGdouble1  hyscan_location_nmea_roll_get           (gchar  *input);

/** Извлекает дифферент из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble1 со значением. */
HyScanLocationGdouble1  hyscan_location_nmea_pitch_get          (gchar  *input);

/** Извлекает скорость из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble1 со значением. */
HyScanLocationGdouble1  hyscan_location_nmea_speed_get          (gchar  *input);

/** Извлекает глубину из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGdouble1 со значением. */
HyScanLocationGdouble1  hyscan_location_nmea_depth_get          (gchar  *input);

/** Извлекает дату и время из NMEA-строки.
 * \param input указатель на строку
 * \return Структура HyScanLocationGint1 со значением. */
HyScanLocationGint1     hyscan_location_nmea_datetime_get       (gchar  *input);

/** Извлекает только время из NMEA-строки.
 * \param input указатель на строку
 * \param sentence_type тип NMEA-строки
 * \return Структура HyScanLocationGdouble2 со значениями.
 */
gint64                  hyscan_location_nmea_time_get           (gchar  *input,
                                                                 HyScanSonarDataType sentence_type);

/* Функции определения глубины по ГБО и эхолоту. */

/** Определяет глубину по эхолоту.
 * \param input указатель на данные;
 * \param input_size размер данных (количество точек);
 * \param discretization_frequency частота дискретизации;
 * \param soundspeedtable указатель на таблицу скорости звука
 * \return Структура HyScanLocationGdouble1 со значением.
 */
HyScanLocationGdouble1  hyscan_location_echosounder_depth_get   (gfloat *input,
                                                                 gint   input_size,
                                                                 gfloat discretization_frequency,
                                                                 GArray *soundspeedtable);
/** Определяет глубину по ГБО.
 * \param input указатель на данные;
 * \param input_size размер данных (количество точек);
 * \param discretization_frequency частота дискретизации;
 * \param soundspeedtable указатель на таблицу скорости звука
 * \return Структура HyScanLocationGdouble1 со значением.
 */
HyScanLocationGdouble1  hyscan_location_sonar_depth_get         (gfloat *input,
                                                                 gint   input_size,
                                                                 gfloat discretization_frequency,
                                                                 GArray *soundspeedtable);

/* Фильтры. */

/** Сглаживает данные кривой Безье.
 * Функция извлекает значения и сглаживает их. Предполагается, что первые две точки уже сглажены,
 * а третья и четвертая - нет. Алгоритм учитывает время, в которое получена каждая точка.
 *
 * <b>Важно:</b> считается, что сглаживаемая точка - третья. Сглаженное значение будет помещено в локальный кэш вместо третьей точки.
 *
 * Параметр quality позволяет задать степень сглаживания: при 0 данные сглаживаются сильней, при 1 слабей.
 * \param source указатель на локальный кэш с данными;
 * \param point1 первая точка;
 * \param point2 вторая точка;
 * \param point3 третья точка;
 * \param point4 четвертая точка;
 * \param quality качество (степень сглаживания).
 */
void                    hyscan_location_4_point_2d_bezier       (GArray *source,
                                                                 gint32 point1,
                                                                 gint32 point2,
                                                                 gint32 point3,
                                                                 gint32 point4,
                                                                 gdouble quality);
void                    hyscan_location_3_point_2d_bezier       (GArray *source,
                                                                 gint32 point1,
                                                                 gint32 point2,
                                                                 gint32 point3,
                                                                 gdouble quality);

/** Среднее арифметическое трех точек.
 *
 * <b>Важно:</b> считается, что сглаживаемая точка - вторая. Сглаженное значение будет помещено в локальный кэш вместо второй точки.
 *
 * Параметр quality позволяет задать степень сглаживания: при 0 данные сглаживаются сильней, при 1 слабей.
 * \param source указатель на локальный кэш с данными;
 * \param point1 первая точка;
 * \param point2 вторая точка;
 * \param point3 третья точка;
 * \param quality качество (степень сглаживания).
 */
void                    hyscan_location_3_point_average         (GArray *source,
                                                                 gint32 point1,
                                                                 gint32 point2,
                                                                 gint32 point3,
                                                                 gdouble quality);

/** Сдвижка данных в пространстве.
 *
 * Поскольку местоположение запрашивается для момента времени \b и для точки, смещенной от центра масс,
 * требуется сдвигать известные данные на известные значения сдвижек. При этом необходимо учитывать курс, крен и дифферент судна.
 * В функцию передаются сдвижки, измеренные в нормальном положении (нулевой курс, крен и дифферент).

 * \param[out] data данные, которые необходимо сдвинуть;
 * \param x сдвижка по оси от центра масс к носу;
 * \param y сдвижка по оси от центра масс к правому борту;
 * \param z сдвижка по оси от центра масс к килю;
 * \param psi угловой сдвиг (в радианах), соответствующий курсу;
 * \param gamma угловой сдвиг (в радианах), соответствующий крену;
 * \param theta угловой сдвиг (в радианах), соответствующий дифференту.
 */
void                    hyscan_location_shift                   (HyScanLocationData *data,
                                                                 gdouble             x,
                                                                 gdouble             y,
                                                                 gdouble             z,
                                                                 gdouble             psi,
                                                                 gdouble             gamma,
                                                                 gdouble             theta);
/* Вспомогательные функции надзирателя. */

/** Функция слежения за широтой и долготой.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
void                    hyscan_location_overseer_latlong         (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *local_cache,
                                                                  gint32 source,
                                                                  GArray *datetime_cache,
                                                                  gint32 datetime_source,
                                                                  gdouble quality);

/** Функция слежения за высотой.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
void                    hyscan_location_overseer_altitude        (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  GArray *datetime_cache,
                                                                  gint32 datetime_source,
                                                                  gdouble quality);

/** Функция слежения за курсом.
 *
 * Функция автоматически определяет по списку источников, какие данные извлекать: курс или координаты.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
void                    hyscan_location_overseer_track           (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  GArray *datetime_cache,
                                                                  gint32 datetime_source,
                                                                  gdouble quality);

/** Функция слежения за креном.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
void                    hyscan_location_overseer_roll            (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  GArray *datetime_cache,
                                                                  gint32 datetime_source,
                                                                  gdouble quality);
/** Функция слежения за дифферентом.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
 void                    hyscan_location_overseer_pitch           (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  GArray *datetime_cache,
                                                                  gint32 datetime_source,
                                                                  gdouble quality);
/** Функция слежения за скоростью.
 *
 * Функция автоматически определяет по списку источников, какие данные извлекать: скорость или координаты.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
 void                    hyscan_location_overseer_speed           (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  GArray *datetime_cache,
                                                                  gint32 datetime_source,
                                                                  gdouble quality);
/** Функция слежения за глубиной.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
void                    hyscan_location_overseer_depth           (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  GArray *soundspeed,
                                                                  gdouble quality);
/** Функция слежения за датой и временем.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 */
void                    hyscan_location_overseer_datetime        (HyScanDB *db,
                                                                  GArray *source_list,
                                                                  GArray *cache,
                                                                  gint32 source,
                                                                  gdouble quality);

/* Функции для аппроксимации и выдачи данных. */

/** Функция выдачи широты и долготы для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
HyScanLocationGdouble2   hyscan_location_getter_latlong          (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи высоты для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
HyScanLocationGdouble1   hyscan_location_getter_altitude         (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи курса для заданного момента времени.
 *
 * Функция умная и сама определяет, что из себя представляют исходные данные (курс или координаты).
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
 HyScanLocationGdouble1   hyscan_location_getter_track            (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи крена для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
 HyScanLocationGdouble1   hyscan_location_getter_roll             (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи дифферента для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
 HyScanLocationGdouble1   hyscan_location_getter_pitch            (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи скорости для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
HyScanLocationGdouble1   hyscan_location_getter_speed            (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи глубины для заданного момента времени.
 *
 * Функция автоматически определяет, в каком виде хранятся данные (скорость или координаты).
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
 HyScanLocationGdouble1   hyscan_location_getter_depth            (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Функция выдачи даты и времени для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 */
 HyScanLocationGint1      hyscan_location_getter_datetime         (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

/** Внутренняя функция выдачи значений типа HyScanLocationGdouble2 для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество;
 * \param[out] prev_point предыдущая точка, относительно которой нужно вычислять курс или скорость.
 */
HyScanLocationGdouble2   hyscan_location_getter_gdouble2         (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality,
                                                                   HyScanLocationGdouble2  *prev_point);

/** Внутренняя функция выдачи значений типа HyScanLocationGdouble1 для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param local_cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество;
 */
 HyScanLocationGdouble1   hyscan_location_getter_gdouble1         (HyScanDB *db,
                                                                   GArray *source_list,
                                                                   GArray *cache,
                                                                   gint32 source,
                                                                   gint64 time,
                                                                   gdouble quality);

G_END_DECLS
#endif /* __HYSCAN_LOCATION_TOOLS_H__ */

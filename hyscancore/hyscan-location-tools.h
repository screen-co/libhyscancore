/*
 * \file hyscan-location-tools.h
 *
 * \brief Заголовочный файл вспомогательных функций и структур для класса HyScanLocation
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanLocationTools HyScanLocationTools - вспомогательные функции и структуры для класса HyScanLocation
 *
 * Эта библиотека функций предназначена исключительно для работы класса HyScanLocation. Отдельно её подключать нельзя.
 * Содержит следующие структуры данных:
 *
 * - #HyScanLocationSourcesList - внутренний список источников;
 * - #HyScanLocationInternalData - контейнер для данных;
 * - #HyScanLocationInternalTime - контейнер для данных времени.
 *
 * И следующие функции:
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
 * Для фильтрации и обработки данных:
 * - #hyscan_location_4_point_2d_bezier - сглаживание кривой Безье;
 * - #hyscan_location_thresholder - линеаризация трека;
 * - #hyscan_location_thresholder2 - линеаризация трека (альтернативная);
 * - #hyscan_location_shift - для сдвижки данных в пространстве с учетом курса, крена и дифферента;
 * - #hyscan_location_track_calculator - вычисление курса;
 * - #hyscan_location_speed_calculator - вычисление скорости;
 * - #hyscan_location_find_time - поиск данных по времени;
 * - #hyscan_location_find_data - поиск данных по времению
 *
 * Функции надзирателя:
 * - #hyscan_location_overseer_latlong - следит за данными широты/долготы;
 * - #hyscan_location_overseer_altitude - следит за данными высоты;
 * - #hyscan_location_overseer_track - следит за данными курса;
 * - #hyscan_location_overseer_roll - следит за данными крена;
 * - #hyscan_location_overseer_pitch - следит за данными дифферента;
 * - #hyscan_location_overseer_speed - следит за данными скорости;
 * - #hyscan_location_overseer_depth - следит за данными глубины;
 * - #hyscan_location_overseer_datetime - следит за данными даты и времени.
 *
 * Функции сборки данных из БД:
 * - #hyscan_location_assembler_latlong - сборка данных широты/долготы;
 * - #hyscan_location_assembler_altitude - сборка данных высоты;
 * - #hyscan_location_assembler_track - сборка данных курса;
 * - #hyscan_location_assembler_roll - сборка данных крена;
 * - #hyscan_location_assembler_pitch - сборка данных дифферента;
 * - #hyscan_location_assembler_speed - сборка данных скорости;
 * - #hyscan_location_assembler_depth - сборка данных глубины;
 * - #hyscan_location_assembler_datetime - сборка данных даты и времени.
 *
 * Функции принудительной установки значений:
 * - #hyscan_location_editor_latlong - установка широты/долготы;
 * - #hyscan_location_editor_track - установка курса;
 * - #hyscan_location_editor_roll - установка крена;
 * - #hyscan_location_editor_pitch - установка дифферента;
 * - #hyscan_location_editor_remove - удаление точек.
 *
 * Функции-getter'ы:
 * - #hyscan_location_getter_latlong - аппроксимация и выдача данных широты/долготы;
 * - #hyscan_location_getter_altitude - аппроксимация и выдача данных высоты;
 * - #hyscan_location_getter_track - аппроксимация и выдача данных курса;
 * - #hyscan_location_getter_roll - аппроксимация и выдача данных крена;
 * - #hyscan_location_getter_pitch - аппроксимация и выдача данных дифферента;
 * - #hyscan_location_getter_speed - аппроксимация и выдача данных скорости;
 * - #hyscan_location_getter_depth - аппроксимация и выдача данных глубины;
 * - #hyscan_location_getter_datetime - аппроксимация и выдача данных даты;
 * - #hyscan_location_getter_gdouble2 - внутренняя функция для получения данных типа HyScanLocationInternalData;
 * - #hyscan_location_getter_gdouble1 - внутренняя функция для получения данных типа HyScanLocationInternalData.
 */
#ifndef __HYSCAN_LOCATION_TOOLS_H__
#define __HYSCAN_LOCATION_TOOLS_H__

#include <hyscan-location.h>
#include <hyscan-core-types.h>
#include <math.h>
#include <string.h>

G_BEGIN_DECLS;

#define UNIX_1200 43200*1e6           /* Unix timestamp, соответствующий 12:00 в микросекундах. */
#define UNIX_2300 82800*1e6           /* Unix timestamp, соответствующий 23:00 в микросекундах. */
#define ONE_DEG_LENGTH 111321.378     /* Длина одного радиана по экватору в метрах. */
#define TIME_OF_VALIDITY 10e3         /* Окно валидности. Если запрошенное время попадает в окно валидности некоторой точки,
                                       * то данные можно не интерполировать, а взять значение этой точки. */
#define ONE_RAD_LENGTH 6474423.1      /* Длина одного радиана по экватору в метрах. */
#define DEPTH_MAXPEAKS 10             /* Максимальное количество пиков для определения глубины. */
#define HYSCAN_LOCATION_INTERNALTIME_INIT 0,0,0,FALSE /* Инициализация переменных типа HyScanLocationInternalTime.*/
#define HYSCAN_LOCATION_INTERNALDATA_INIT 0,0,NAN,NAN,NAN,FALSE /* Инициализация переменных типа HyScanLocationInternalData.*/
#define HYSCAN_LOCATIONDATA_NAN NAN,NAN,NAN,0,0,0,NAN,NAN,0,0 /* Инициализация переменных типа HyScanLocationData.*/

/** \brief Флаги валидности данных */
typedef enum
{
  HYSCAN_LOCATION_INVALID      = 0,                     /**< Данные не валидны. */
  HYSCAN_LOCATION_PARSED       = 1,                     /**< Произведена сборка данных. */
  HYSCAN_LOCATION_ASSEMBLED    = 2,                     /**< Произведена сборка данных. */
  HYSCAN_LOCATION_PREPROCESSED = 3,                     /**< Произведена предобработка данных. */
  HYSCAN_LOCATION_PROCESSED    = 4,                     /**< Произведена обработка данных. */
  HYSCAN_LOCATION_USER_INVALID = 5,                     /**< Данные принудительно заданы пользователем. */
  HYSCAN_LOCATION_USER_VALID   = 6,                     /**< Данные принудительно заданы пользователем. */
  HYSCAN_LOCATION_VALID        = 7,                     /**< Данные валидны. */
} HyScanLocationValidity;

/** \brief Внутренняя таблица источников данных */
typedef struct
{
  gint32 index;                                         /**< Индекс источника. */

  HyScanLocationParameters  parameter;                  /**< Параметр, обрабатываемый источником. */
  HyScanLocationSourceTypes source_type;                /**< Тип источника. */
  HyScanSonarChannelIndex   sensor_channel;             /**< Номер КД. */
  gboolean                  active;                     /**< Используется ли источник. */

  HyScanDataChannel        *dchannel;                   /**< Для работы с акустическими данными. */
  gchar                    *channel_name;               /**< Имя КД (для работы с не-акустическими данными). */
  gint32                    channel_id;                 /**< Идентификатор открытого КД (для работы с не-акустическими данными). */
  gint32                    param_id;                   /**< Идентификатор параметров КД (для работы с не-акустическими данными). */

  gint32                    shift;                      /**< Сдвиг (по сути, индекс самого первого элемента в КД). */
  gint32                    db_index;                   /**< Индекс данных в БД. */
  gint32                    assembler_index;            /**< Индекс сборщика данных. */
  gint32                    preprocessing_index;        /**< Индекс предобработчика данных. */
  gint32                    thresholder_prev_index;     /**< Индекс предыдущей точки для функции #hyscan_location_thresholder2. */
  gint32                    thresholder_next_index;     /**< Индекс следующей точки для функции #hyscan_location_thresholder2. */
  gint32                    processing_index;           /**< Индекс обработчика данных. */

  gdouble                   x;                          /**< Параметр датчика. */
  gdouble                   y;                          /**< Параметр датчика. */
  gdouble                   z;                          /**< Параметр датчика. */
  gdouble                   psi;                        /**< Параметр датчика. */
  gdouble                   gamma;                      /**< Параметр датчика. */
  gdouble                   theta;                      /**< Параметр датчика. */
  gdouble                   discretization_frequency;   /**< Параметр датчика. */
  gchar                    *discretization_type;        /**< Параметр датчика. */
} HyScanLocationSourcesList;

/** \brief Внутренняя структура. Иcпользуется для хранения любых данных. */
typedef struct
{
  gint64                    db_time;                    /**< Время, в которое данные записались в БД. */
  gint64                    data_time;                  /**< Время, содержащееся в самих данных. */
  gdouble                   int_latitude;               /**< Широта. */
  gdouble                   int_longitude;              /**< Долгота. */
  gdouble                   int_value;                  /**< Курс, скорость... */
  gboolean                  validity;                   /**< Флаг валидности данных. */
} HyScanLocationInternalData;

/** \brief Внутренняя структура. Иcпользуется для работы с данными времени. */
typedef struct
{
  gint64                    db_time;                    /**< Время, в которое данные записались в БД. */
  gint64                    date;                       /**< Дата. */
  gint64                    time;                       /**< Время. */
  gint64                    time_shift;                 /**< Временная сдвижка. */
  gboolean                  validity;                   /**< Флаг валидности данных. */
} HyScanLocationInternalTime;

typedef enum
{
  HYSCAN_LOCATION_EDIT_LATLONG = 1,
  HYSCAN_LOCATION_EDIT_TRACK,
  HYSCAN_LOCATION_EDIT_ROLL,
  HYSCAN_LOCATION_EDIT_PITCH,
  HYSCAN_LOCATION_BULK_EDIT,
  HYSCAN_LOCATION_REMOVE,
  HYSCAN_LOCATION_BULK_REMOVE
} HyScanLocationUserParameterTypes;

typedef struct
{
  gint    type;
  gint64  ltime;
  gint64  rtime;
  gdouble value1;
  gdouble value2;
  gdouble value3;
  gdouble value4;
} HyScanLocationUserParameters;

/* Функции работы с NMEA-строками. */

/**
 *
 * Извлекает широту и долготу из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значениями.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_latlong_get        (gchar  *input);

/**
 *
 * Извлекает высоту из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_altitude_get       (gchar  *input);

/**
 *
 * Извлекает курс из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_track_get          (gchar  *input);

/**
 *
 * Извлекает крен из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_roll_get           (gchar  *input);

/**
 *
 * Извлекает дифферент из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_pitch_get          (gchar  *input);

/**
 *
 * Извлекает скорость из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_speed_get          (gchar  *input);

/**
 *
 * Извлекает глубину из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_nmea_depth_get          (gchar  *input);

/**
 *
 * Извлекает дату и время из NMEA-строки.
 *
 * \param input указатель на строку
 * \return Структура HyScanLocationInternalTime со значением.
 *
 */
HyScanLocationInternalTime     hyscan_location_nmea_datetime_get       (gchar  *input);

/**
 *
 * Извлекает только время из NMEA-строки.
 *
 * \param input указатель на строку
 * \param sentence_type тип NMEA-строки
 * \return Структура HyScanLocationInternalData со значениями.
 *
 */
gint64                  hyscan_location_nmea_time_get           (gchar              *input,
                                                                 HyScanSonarDataType sentence_type);

/* Функции определения глубины по ГБО и эхолоту. */

/**
 *
 * Определяет глубину по эхолоту.
 *
 * \param input указатель на данные;
 * \param input_size размер данных (количество точек);
 * \param discretization_frequency частота дискретизации;
 * \param soundspeedtable указатель на таблицу скорости звука
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_echosounder_depth_get   (gfloat *input,
                                                                 gint    input_size,
                                                                 gfloat  discretization_frequency,
                                                                 GArray *soundspeedtable);
/**
 *
 * Определяет глубину по ГБО.
 *
 * \param input указатель на данные;
 * \param input_size размер данных (количество точек);
 * \param discretization_frequency частота дискретизации;
 * \param soundspeedtable указатель на таблицу скорости звука
 * \return Структура HyScanLocationInternalData со значением.
 *
 */
HyScanLocationInternalData  hyscan_location_sonar_depth_get         (gfloat *input,
                                                                 gint    input_size,
                                                                 gfloat  discretization_frequency,
                                                                 GArray *soundspeedtable);

/* Фильтры. */

/**
 *
 * Сглаживает данные кривой Безье.
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
 *
 */
void                    hyscan_location_4_point_2d_bezier       (GArray *source,
                                                                 gint32  point1,
                                                                 gint32  point2,
                                                                 gint32  point3,
                                                                 gint32  point4,
                                                                 gdouble quality);
/**
 *
 * Линеаризация данных.
 *
 * Функция предназначена для "нарезки" трэка на прямолинейные участки. Длина участков постоянна и определяется параметром quality
 * и варьируется от 1 до 10 метров.
 * <b>Важно:</b> считается, что сглаживаемая точка - вторая.
 *
 * \param source указатель на локальный кэш с данными;
 * \param point1 первая точка текущего прямолинейного участка;
 * \param point2 обрабатываемая точка;
 * \param point3 последняя точка текущего прямолинейного участка;
 * \param last_index индекс последней точки, обработанной на этапе предобработки.
 * \param is_writeable сигнализирует, что данных больше не будет.
 * \param quality качество (степень сглаживания).
 *
 */
gboolean                hyscan_location_thresholder             (GArray  *source,
                                                                 gint32  *point1,
                                                                 gint32   point2,
                                                                 gint32  *point3,
                                                                 gint32   last_index,
                                                                 gboolean is_writeable,
                                                                 gdouble  quality);

/**
 *
 * Линеаризация данных.
 *
 * Функция предназначена для "нарезки" трэка на прямолинейные участки.
 * Отличие от #hyscan_location_thresholder в том, что длина участков переменна,
 * а функция стремится минимизировать изменение курса.
 * Считается, что сглаживаемая точка - третья.
 *
 * \param source указатель на локальный кэш с данными;
 * \param point2 первая точка текущего прямолинейного участка;
 * \param point3 обрабатываемая точка;
 * \param point4 последняя точка текущего прямолинейного участка;
 * \param last_index индекс последней точки, обработанной на этапе предобработки.
 * \param is_writeable сигнализирует, что данных больше не будет.
 * \param quality качество (степень сглаживания).
 *
 */
gboolean                hyscan_location_thresholder2            (GArray  *source,
                                                                 gint32  *point2,
                                                                 gint32   point3,
                                                                 gint32  *point4,
                                                                 gint32   last_index,
                                                                 gboolean is_writeable,
                                                                 gdouble  quality);

/**
 *
 * Сдвижка данных во времени
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param source активный источник;
 * \param cache локальный кэш с данными;
 * \param datetime_source источник временных данных;
 * \param datetime_cache кэш временных данных;
 * \param index индекс точки в локальном кэше.
 *
 */
void                    hyscan_location_timeshift               (HyScanDB *db,
                                                                 GArray *source_list,
                                                                 gint32  source,
                                                                 GArray *cache,
                                                                 gint32  datetime_source,
                                                                 GArray *datetime_cache,
                                                                 gint32  index);

/**
 *
 * Сдвижка данных в пространстве.
 *
 * Поскольку местоположение запрашивается для момента времени \b и для точки, смещенной от центра масс,
 * требуется сдвигать известные данные на известные значения сдвижек. При этом необходимо учитывать курс, крен и дифферент судна.
 * В функцию передаются сдвижки, измеренные в нормальном положении (нулевой курс, крен и дифферент).
 *
 * \param[out] data данные, которые необходимо сдвинуть;
 * \param x сдвижка по оси от центра масс к носу;
 * \param y сдвижка по оси от центра масс к правому борту;
 * \param z сдвижка по оси от центра масс к килю;
 * \param psi угловой сдвиг (в радианах), соответствующий курсу;
 * \param gamma угловой сдвиг (в радианах), соответствующий крену;
 * \param theta угловой сдвиг (в радианах), соответствующий дифференту.
 *
 */
void                    hyscan_location_shift                   (HyScanLocationData *data,
                                                                 gdouble             x,
                                                                 gdouble             y,
                                                                 gdouble             z,
                                                                 gdouble             psi,
                                                                 gdouble             gamma,
                                                                 gdouble             theta);

/**
 *
 * Сдвижка данных в пространстве.
 *
 * Поскольку местоположение запрашивается для момента времени \b и для точки, смещенной от центра масс,
 * требуется сдвигать известные данные на известные значения сдвижек. При этом необходимо учитывать курс, крен и дифферент судна.
 * В функцию передаются сдвижки, измеренные в нормальном положении (нулевой курс, крен и дифферент).
 *
 * \param[out] data данные, которые необходимо сдвинуть;
 * \param x сдвижка по оси от центра масс к носу;
 * \param y сдвижка по оси от центра масс к правому борту;
 * \param z сдвижка по оси от центра масс к килю;
 * \param psi угловой сдвиг (в радианах), соответствующий курсу;
 * \param gamma угловой сдвиг (в радианах), соответствующий крену;
 * \param theta угловой сдвиг (в радианах), соответствующий дифференту.
 *
 */
void                    hyscan_location_shift2                  (HyScanLocationData *data,
                                                                 gdouble             x,
                                                                 gdouble             y,
                                                                 gdouble             z,
                                                                 gdouble             psi,
                                                                 gdouble             gamma,
                                                                 gdouble             theta);

/**
 *
 * Функция вычисления курса.
 *
 * Вспомогательная функция, вычисляет скорость по координатам двух точек и времени между ними.
 *
 * \param lat1 широта первой точки в десятичных градусах;
 * \param lon1 долгота первой точки в десятичных градусах;
 * \param lat2 широта второй точки в десятичных градусах;
 * \param lon2 долгота второй точки в десятичных градусах;
 * \return значение курса в градусах. 0 соответствует северу.
 *
 */
gdouble                 hyscan_location_track_calculator        (gdouble lat1,
                                                                 gdouble lon1,
                                                                 gdouble lat2,
                                                                 gdouble lon2);

/**
 *
 * Функция вычисления курса.
 *
 * Вспомогательная функция, вычисляет курс по координатам двух точек.
 *
 * \param lat1 широта первой точки в десятичных градусах;
 * \param lon1 долгота первой точки в десятичных градусах;
 * \param lat2 широта второй точки в десятичных градусах;
 * \param lon2 долгота второй точки в десятичных градусах;
 * \param time время между точками в микросекундах.
 * \return скорость в м/с.
 *
 */
gdouble                 hyscan_location_speed_calculator        (gdouble lat1,
                                                                 gdouble lon1,
                                                                 gdouble lat2,
                                                                 gdouble lon2,
                                                                 gdouble time);

/**
 *
 * Функция поиска временных данных.
 *
 * Для заданного момента времени функция находит данные в кэше данных времени.
 * Если rindex == lindex, значит, найдены точные данные для запрошенного времени.
 *
 * \param cache локальный кэш данных;
 * \param source_list список источников;
 * \param source активный источник;
 * \param time время, для которого нужно найти данные;
 * \param[out] lindex ближайший индекс данных справа;
 * \param[out] rindex ближайший индекс данных слева.
 * \return TRUE, если данные удалось найти
 *
 */
gboolean                hyscan_location_find_time               (GArray *cache,
                                                                 GArray *source_list,
                                                                 gint32  source,
                                                                 gint64  time,
                                                                 gint32 *lindex,
                                                                 gint32 *rindex);

/**
 *
 * Функция поиска данных.
 *
 * Для заданного момента времени функция находит данные в любом локальном кэше данных (за исключением времени).
 * Такое разделение вызвано тем, что данные времени хранятся в структурах типа HyScanLocationInternalTime, а
 * все остальные данные (координаты, курс, скорость и т.д.) в структурах типа HyScanLocationInternalData.
 * Если rindex == lindex, значит, найдены точные данные для запрошенного времени.
 *
 * \param cache локальный кэш данных;
 * \param source_list список источников;
 * \param source активный источник;
 * \param time время, для которого нужно найти данные;
 * \param[out] lindex ближайший индекс данных справа;
 * \param[out] rindex ближайший индекс данных слева.
 * \param[out] ltime время в данных по ближайшему индекс данных справа;
 * \param[out] rtime время в данных по ближайшему индекс данных слева.
 * \return TRUE, если данные удалось найти
 *
 */

gboolean                hyscan_location_find_data               (GArray *cache,
                                                                 GArray *source_list,
                                                                 gint32  source,
                                                                 gint64  time,
                                                                 gint32 *lindex,
                                                                 gint32 *rindex,
                                                                 gint64 *ltime,
                                                                 gint64 *rtime);

/* Вспомогательные функции надзирателя. */

/**
 *
 * Функция слежения за широтой и долготой.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param params пользовательские параметры обработки;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param datetime_cache указатель на локальный кэш даты и времени;
 * \param datetime_source индекс источника даты и времени;
 * \param quality качество.
 *
 */


void                    hyscan_location_overseer_latlong         (HyScanDB *db,
                                                                  GArray   *source_list,
                                                                  GArray   *params,
                                                                  GArray   *cache,
                                                                  gint32    source,
                                                                  GArray   *datetime_cache,
                                                                  gint32    datetime_source,
                                                                  gdouble   quality);

/**
 *
 * Функция слежения за высотой.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param datetime_cache указатель на локальный кэш даты и времени;
 * \param datetime_source индекс источника даты и времени;
 * \param quality качество.
 *
 */
void                    hyscan_location_overseer_altitude        (HyScanDB *db,
                                                                  GArray   *source_list,
                                                                  GArray   *cache,
                                                                  gint32    source,
                                                                  GArray   *datetime_cache,
                                                                  gint32    datetime_source,
                                                                  gdouble   quality);

/**
 *
 * Функция слежения за курсом.
 *
 * Функция автоматически определяет по списку источников, какие данные извлекать: курс или координаты.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param params пользовательские параметры обработки;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param datetime_cache указатель на локальный кэш даты и времени;
 * \param datetime_source индекс источника даты и времени;
 * \param quality качество.
 *
 */
void                    hyscan_location_overseer_track           (HyScanDB *db,
                                                                  GArray   *source_list,
                                                                  GArray   *params,
                                                                  GArray   *cache,
                                                                  gint32    source,
                                                                  GArray   *datetime_cache,
                                                                  gint32    datetime_source,
                                                                  gdouble   quality);

/**
 *
 * Функция слежения за креном.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param params пользовательские параметры обработки;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param datetime_cache указатель на локальный кэш даты и времени;
 * \param datetime_source индекс источника даты и времени;
 * \param quality качество.
 *
 */
void                    hyscan_location_overseer_roll            (HyScanDB *db,
                                                                  GArray   *source_list,
                                                                  GArray   *params,
                                                                  GArray   *cache,
                                                                  gint32    source,
                                                                  GArray   *datetime_cache,
                                                                  gint32    datetime_source,
                                                                  gdouble   quality);
/**
 *
 * Функция слежения за дифферентом.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param params пользовательские параметры обработки;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param datetime_cache указатель на локальный кэш даты и времени;
 * \param datetime_source индекс источника даты и времени;
 * \param quality качество.
 *
 */
 void                    hyscan_location_overseer_pitch           (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   GArray   *datetime_cache,
                                                                   gint32    datetime_source,
                                                                   gdouble   quality);
/**
 *
 * Функция слежения за скоростью.
 *
 * Функция автоматически определяет по списку источников, какие данные извлекать: скорость или координаты.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param params пользовательские параметры обработки;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param datetime_cache указатель на локальный кэш даты и времени;
 * \param datetime_source индекс источника даты и времени;
 * \param quality качество.
 *
 */
 void                    hyscan_location_overseer_speed           (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   GArray   *datetime_cache,
                                                                   gint32    datetime_source,
                                                                   gdouble   quality);
/**
 *
 * Функция слежения за глубиной.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param soundspeed указатель на таблицу скорости звука;
 * \param quality качество.
 *
 */
void                    hyscan_location_overseer_depth           (HyScanDB *db,
                                                                  GArray   *source_list,
                                                                  GArray   *cache,
                                                                  gint32    source,
                                                                  GArray   *soundspeed,
                                                                  gdouble   quality);
/**
 *
 * Функция слежения за датой и временем.
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param quality качество.
 *
 */
void                    hyscan_location_overseer_datetime        (HyScanDB *db,
                                                                  GArray   *source_list,
                                                                  GArray   *cache,
                                                                  gint32    source,
                                                                  gdouble   quality);

/* Функции для аппроксимации и выдачи данных. */

/**
 *
 * Функция выдачи широты и долготы для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
HyScanLocationInternalData   hyscan_location_getter_latlong          (HyScanDB *db,
                                                                   GArray  *source_list,
                                                                   GArray  *cache,
                                                                   gint32   source,
                                                                   gint64   time,
                                                                   gdouble  quality);

/**
 *
 * Функция выдачи высоты для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
HyScanLocationInternalData   hyscan_location_getter_altitude         (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/**
 *
 * Функция выдачи курса для заданного момента времени.
 *
 * Функция умная и сама определяет, что из себя представляют исходные данные (курс или координаты).
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
 HyScanLocationInternalData   hyscan_location_getter_track            (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/**
 *
 * Функция выдачи крена для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
 HyScanLocationInternalData   hyscan_location_getter_roll             (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/**
 *
 * Функция выдачи дифферента для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
 HyScanLocationInternalData   hyscan_location_getter_pitch            (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/**
 *
 * Функция выдачи скорости для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
HyScanLocationInternalData   hyscan_location_getter_speed            (HyScanDB *db,
                                                                   GArray  *source_list,
                                                                   GArray  *cache,
                                                                   gint32   source,
                                                                   gint64   time,
                                                                   gdouble  quality);

/**
 *
 * Функция выдачи глубины для заданного момента времени.
 *
 * Функция автоматически определяет, в каком виде хранятся данные (скорость или координаты).
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
 HyScanLocationInternalData   hyscan_location_getter_depth            (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/**
 *
 * Функция выдачи даты и времени для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество.
 *
 */
 HyScanLocationInternalTime      hyscan_location_getter_datetime         (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/**
 *
 * Внутренняя функция выдачи значений типа HyScanLocationInternalData для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество;
 * \param[out] prev_point предыдущая точка, относительно которой нужно вычислять курс или скорость.
 *
 */
HyScanLocationInternalData   hyscan_location_getter_gdouble2         (HyScanDB                 *db,
                                                                   GArray                  *source_list,
                                                                   GArray                  *cache,
                                                                   gint32                   source,
                                                                   gint64                   time,
                                                                   gdouble                  quality,
                                                                   HyScanLocationInternalData  *prev_point);

/**
 *
 * Внутренняя функция выдачи значений типа HyScanLocationInternalData для заданного момента времени.
 *
 * \param db указатель на БД;
 * \param source_list список источников;
 * \param cache указатель на локальный кэш;
 * \param source индекс источника;
 * \param time требуемый момент времени;
 * \param quality качество;
 *
 */
 HyScanLocationInternalData   hyscan_location_getter_gdouble1     (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    time,
                                                                   gdouble   quality);

/* Функция сборки даты и времени.*/
HyScanLocationInternalTime    hyscan_location_assembler_datetime  (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки координат.*/
HyScanLocationInternalData    hyscan_location_assembler_latlong   (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки высоты.*/
HyScanLocationInternalData    hyscan_location_assembler_altitude  (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки курса.*/
HyScanLocationInternalData    hyscan_location_assembler_track     (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки крена.*/
HyScanLocationInternalData    hyscan_location_assembler_roll      (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки дифферента.*/
HyScanLocationInternalData    hyscan_location_assembler_pitch     (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки скорости.*/
HyScanLocationInternalData    hyscan_location_assembler_speed     (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *params,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   gint64    index);

/* Функция сборки глубины.*/
HyScanLocationInternalData    hyscan_location_assembler_depth     (HyScanDB *db,
                                                                   GArray   *source_list,
                                                                   GArray   *cache,
                                                                   gint32    source,
                                                                   GArray   *soundspeed,
                                                                   gint64    index);

void                      hyscan_location_editor_latlong          (GArray *cache,
                                                                   gint64  index,
                                                                   gdouble latitude,
                                                                   gdouble longitude);

void                      hyscan_location_editor_track            (GArray *cache,
                                                                   GArray *source_list,
                                                                   gint32  source,
                                                                   gint64  index,
                                                                   gdouble value);

void                      hyscan_location_editor_roll             (GArray *cache,
                                                                   GArray *source_list,
                                                                   gint32  source,
                                                                   gint64  index,
                                                                   gdouble value);

void                      hyscan_location_editor_pitch            (GArray *cache,
                                                                   GArray *source_list,
                                                                   gint32  source,
                                                                   gint64  index,
                                                                   gdouble value);

void                      hyscan_location_editor_remove           (GArray *cache,
                                                                   GArray *source_list,
                                                                   gint32  source,
                                                                   gint64  index);

G_END_DECLS
#endif /* __HYSCAN_LOCATION_TOOLS_H__ */

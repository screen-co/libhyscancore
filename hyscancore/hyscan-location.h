/**
 * \file hyscan-location.h
 *
 * \brief Заголовочный файл класса определения координат
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanLocation HyScanLocation - класс для определения координат и глубины.
 *
 * Класс HyScanLocation предназначен для определения координат, высоты, скорости, курса, крена, дифферента и глубины в требуемый момент времени.
 *
 * В галсе есть каналы данных: эхолоты, ГБО, GPS/GLONASS-приемники, САД.<BR>
 * Каждый КД дает определенный список параметров, которые из него можно извлечь, например, из RMC-строк можно получить информацию
 * о дате, времени, координатах, курсе и скорости. Эти параметры определены в структуре #HyScanLocationParameters.
 *
 * При работе с каналами данных и параметрами, класс оперирует понятием источник.
 * Источник - это некий номер, позволяющий однозначно сопоставить параметр обработки и канал данных.<BR>
 * Класс хранит информацию об источниках в таблице HyScanLocationSourcesList. Это внутренняя таблица, повлиять на которую извне невозможно,
 * однако можно получить список источников для параметра навигационных данных с помощью функции #hyscan_location_sources_list.
 * Так же можно получить индекс активного источника для параметра обработки с помощью функции #hyscan_location_source_get.
 * Установка источника производится с помощью функции #hyscan_location_source_set.
 *
 * Работа класса разделена на три этапа. <BR>
 * На первом этапе считывается список КД в галсе, составляется список источников,
 * устанавливаются источники по умолчанию, инициализируются локальные кэши, запускается поток-надзиратель.
 *
 * Работа потока-надзирателя - это второй этап работы класса.
 * Этот поток постоянно проверяет БД на предмет наличия новых данных, разбирает их и
 * складывает в локальные кэши (для каждого параметра свой кэш). При этом может производиться предобработка данных,
 * например, координаты сглаживаются кривыми Безье.
 *
 * Третий этап - это получение информации о местоположении судна. Для этого пользователем вызывается метод hyscan_location_get.
 * В этом методе последовательно вызываются функции-getter'ы для каждого параметра,
 * который интересует пользователя. Getter'ы аппроксимируют данные (если требуется) и выдают их наверх.
 *
 * Публично доступны следующие методы:
 * - #hyscan_location_sources_list - список доступных источников для заданного параметра;
 * - #hyscan_location_source_get - выбранный источник для заданного параметра;
 * - #hyscan_location_source_set - установка источника для заданного параметра;
 * - #hyscan_location_soundspeed_set - установка профиля скорости звука;
 * - #hyscan_location_get - получение информации о местоположении;
 * - #hyscan_location_get_mod_count - счетчик изменений;
 * - #hyscan_location_get_progress - процент выполнения расчетов (режим пост-обработки)
 *
 * - - -
 * Объект использует библиотеку функций \link HyScanLocationTools \endlink
 * \htmlonly
 * <details>
 *  <summary>Дополнительная информация о таблице скорости звука</summary>
 *  Под профилем скорости звука понимается таблично заданная функция скорости звука от глубины, например, такая:<br>
 *  <table style="border-collapse:collapse;border-spacing:0"><tr><th style="font-size:14px;font-weight:bold;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;background-color:#329a9d;text-align:center;vertical-align:top">Глубина</th><th style="font-family:Arial, sans-serif;font-size:14px;font-weight:bold;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;background-color:#329a9d;text-align:center;vertical-align:top">Скорость звука</th></tr><tr><td style="font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;text-align:center;vertical-align:top">0</td><td style="font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;text-align:center;vertical-align:top">1500</td></tr><tr><td style="font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;text-align:center;vertical-align:top">2</td><td style="font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;text-align:center;vertical-align:top">1450</td></tr><tr><td style="font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;text-align:center;vertical-align:top">4</td><td style="font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;text-align:center;vertical-align:top">1400</td></tr></table>
 *  Где глубина задается в метрах, а скорость звука в метрах в секунду. <br>
 *  Данный пример говорит о следующем: на глубинах от 0 до 2 метров скорость звука составляет 1500 м/с,
 *  от 2 до 4 метров - 1450 м/с, от 4 и далее - 1400 м/с. <br>
 *  Глубина обязательно должна начинаться от нуля. <br>
 *  Таблица представляет собой GArray, состоящий из структур типа SoundSpeedTable.
 * </details>
 * \endhtmlonly
 */
#ifndef __HYSCAN_LOCATION_H__
#define __HYSCAN_LOCATION_H__

#include <glib-object.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>
#include <hyscan-data-channel.h>
#include <glib/gprintf.h>
#include <math.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_LOCATION             (hyscan_location_get_type ())
#define HYSCAN_LOCATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_LOCATION, HyScanLocation))
#define HYSCAN_IS_LOCATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_LOCATION))
#define HYSCAN_LOCATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_LOCATION, HyScanLocationClass))
#define HYSCAN_IS_LOCATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_LOCATION))
#define HYSCAN_LOCATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_LOCATION, HyScanLocationClass))

/** \brief Параметры, обрабатываемые объектом. */
typedef enum
{
  HYSCAN_LOCATION_PARAMETER_LATLONG       = 1 << 0,     /**< Широта. */
  HYSCAN_LOCATION_PARAMETER_ALTITUDE      = 1 << 1,     /**< Долгота. */
  HYSCAN_LOCATION_PARAMETER_TRACK         = 1 << 2,     /**< Курс. */
  HYSCAN_LOCATION_PARAMETER_ROLL          = 1 << 3,     /**< Крен. */
  HYSCAN_LOCATION_PARAMETER_PITCH         = 1 << 4,     /**< Дифферент. */
  HYSCAN_LOCATION_PARAMETER_SPEED         = 1 << 5,     /**< Скорость. */
  HYSCAN_LOCATION_PARAMETER_DEPTH         = 1 << 6,     /**< Глубина. */
  HYSCAN_LOCATION_PARAMETER_DATETIME      = 1 << 7,     /**< Дата и время. */
} HyScanLocationParameters;

/** \brief Типы источников. */
typedef enum
{
  HYSCAN_LOCATION_SOURCE_NMEA,                          /**< NMEA-строки. При этом информация непосредственно берется из строки. */
  HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED,                 /**< NMEA-строки, но значение вычисляется из других значений (курс и скорость из координат). */
  HYSCAN_LOCATION_SOURCE_ECHOSOUNDER,                   /**< Эхолот. */
  HYSCAN_LOCATION_SOURCE_SONAR_PORT,                    /**< ГБО, левый борт. */
  HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD,               /**< ГБО, правый борт. */
  HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT,              /**< ГБО, высокое разрешение, левый борт. */
  HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD,         /**< ГБО, высокое разрешение, правый борт. */
  HYSCAN_LOCATION_SOURCE_SAS
} HyScanLocationSourceTypes;

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
  gint32                    assembler_index;            /**< Индекс сборщика данных. */
  gint32                    preprocessing_index;        /**< Индекс предобработчика данных. */
  gint32                    thresholder_prev_index;     /**< Индекс предыдущей точки для функции #hyscan_location_thresholder. */
  gint32                    thresholder_next_index;     /**< Индекс следующей точки для функции #hyscan_location_thresholder. */
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

/** \brief Таблица источников данных, отдаваемая при вызове  #hyscan_location_sources_list */
typedef struct
{
  gint                      index;                      /**< Индекс источника. */
  HyScanLocationSourceTypes source_type;                /**< Тип источника. */
  HyScanSonarChannelIndex   sensor_channel;             /**< Номер КД. */
} HyScanLocationSources;

/** \brief Выходная структура, отдаваемая при вызове  #hyscan_location_get */
typedef struct
{
  gdouble                   latitude;                   /**< Широта. */
  gdouble                   longitude;                  /**< Долгота. */
  gdouble                   altitude;                   /**< Высота. */
  gdouble                   track;                      /**< Курс. */
  gdouble                   roll;                       /**< Крен. */
  gdouble                   pitch;                      /**< Дифферент. */
  gdouble                   speed;                      /**< Скорость. */
  gdouble                   depth;                      /**< Глубина. */
  gint64                    time;                       /**< Время. */
  gboolean                  validity;                   /**< Признак валидности данных. */
} HyScanLocationData;

/** \brief Внутренняя структура. Иcпользуется для работы с данными широты и долготы. */
typedef struct
{
  gint64                    db_time;                    /**< Время, в которое данные записались в БД. */
  gint64                    data_time;                  /**< Время, содержащееся в самих данных. */
  gdouble                   value1;                     /**< Значение 1 (обычно - широта). */
  gdouble                   value2;                     /**< Значение 2 (обычно - долгота). */
  gboolean                  validity;                   /**< Флаг валидности данных. */
} HyScanLocationGdouble2;

/** \brief Внутренняя структура. Иcпользуется для работы с данными высоты, курса, крена, дифферента, глубины. */
typedef struct
{
  gint64                    db_time;                    /**< Время, в которое данные записались в БД. */
  gint64                    data_time;                  /**< Время, содержащееся в самих данных. */
  gdouble                   value;                      /**< Значение. */
  gboolean                  validity;                   /**< Флаг валидности данных. */
} HyScanLocationGdouble1;

/** \brief Внутренняя структура. Иcпользуется для работы с данными времени. */
typedef struct
{
  gint64                    db_time;                    /**< Время, в которое данные записались в БД. */
  gint64                    date;                       /**< Дата. */
  gint64                    time;                       /**< Время. */
  gint64                    time_shift;                 /**< Временная сдвижка. */
  gboolean                  validity;                   /**< Флаг валидности данных. */
} HyScanLocationGint1;

/** \brief Таблица профиля скорости звука. */
typedef struct
{
  gdouble                   depth;
  gdouble                   soundspeed;
} SoundSpeedTable;

typedef struct _HyScanLocation HyScanLocation;
typedef struct _HyScanLocationPrivate HyScanLocationPrivate;
typedef struct _HyScanLocationClass HyScanLocationClass;

struct _HyScanLocation
{
  GObject parent_instance;

  HyScanLocationPrivate *priv;
};

struct _HyScanLocationClass
{
  GObjectClass parent_class;
};

GType                   hyscan_location_get_type                (void);

/**
 * Функция возвращает список источников для заданного параметра.
 *
 * После использования необходимо освободить занятую списком память.
 * На последнем месте в списке источников находится структура со всеми полями, установленными в 0.
 * Это признак конца списка источников.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param parameter параметр из #HyScanLocationParameters.
 *
 * \return указатель на массив со списком источников.
 */
HYSCAN_CORE_EXPORT
HyScanLocationSources  *hyscan_location_sources_list            (HyScanLocation *location,
                                                                 gint            parameter);

/**
 *
 * Функция возвращает активный источник для заданного параметра.
 *
 * \param location указатель на объект обработки навигационных данных
 * \param parameter параметр из #HyScanLocationParameters.
 *
 * \return указатель на массив со списком источников.
 */
HYSCAN_CORE_EXPORT
gint                    hyscan_location_source_get              (HyScanLocation *location,
                                                                 gint            parameter);

/**
 * Функция устанавливает источник.
 *
 * При вызове #hyscan_location_sources_list возвращается список источников,
 * в котором каждому источнику присвоен индекс.
 * Этот индекс передается в hyscan_location_source_set, которая автоматически определяет,
 * для какого параметра актуален этот источник.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param source индекс источника данных;
 * \param turn_on если FALSE, то параметр, соответствующий этому источнику, исключается из обработки.
 *
 * \return TRUE, если источник корректно установился.
 */
HYSCAN_CORE_EXPORT
gboolean                hyscan_location_source_set              (HyScanLocation *location,
                                                                 gint            source,
                                                                 gboolean        turn_on);


/**
 * Функция устанавливает таблицу профиля скорости звука.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param soundspeedtable таблица скорости звука #SoundSpeedTable.
 *
 * \return TRUE, всегда.
 */
HYSCAN_CORE_EXPORT
gboolean                hyscan_location_soundspeed_set          (HyScanLocation *location,
                                                                 GArray          soundspeedtable);

/**
 * Функция получения координат в заданный момент времени.
 *
 * В выходной структуре для выбранных параметров будут указаны значения, для невыбранных - NAN.
 * Возможна ситуация, при которой для заданного момента времени нет данных (либо не существуют, либо еще не обработаны).
 * В таком случае будут возвращены все собранные данные, но те данные, что собрать не удалось, будут установлены в NAN. Дополнительно флаг validity будет установлен в FALSE.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param parameter параметр из #HyScanLocationParameters. Параметры можно объединять логическим ИЛИ;
 * \param time время;
 * \param x сдвижка по оси от центра масс к носу;
 * \param y сдвижка по оси от центра масс к правому борту;
 * \param z сдвижка по оси от центра масс к килю;
 * \param psi угловой сдвиг (в радианах), соответствующий курсу;
 * \param gamma угловой сдвиг (в радианах), соответствующий крену;
 * \param theta угловой сдвиг (в радианах), соответствующий дифференту.

 * \return Структура #HyScanLocationData со всей затребованной информацией.
 */
HYSCAN_CORE_EXPORT
HyScanLocationData      hyscan_location_get                     (HyScanLocation *location,
                                                                 gint            parameter,
                                                                 gint64          time,
                                                                 gdouble         x,
                                                                 gdouble         y,
                                                                 gdouble         z,
                                                                 gdouble         psi,
                                                                 gdouble         gamma,
                                                                 gdouble         theta);
/**
 * Функция возвращает номер изменения в объекте.
 * Номер изменения увеличивается, например, при изменении параметров обработки.
 *
 * Программа не должна полагаться на значение номера изменения, важен только факт смены номера по
 * сравнению с предыдущим запросом.
 *
 * \param location указатель на объект обработки навигационных данных.
 *
 * \return Номер изменения.
 *
 */
HYSCAN_CORE_EXPORT
gint32                  hyscan_location_get_mod_count           (HyScanLocation *location);
HYSCAN_CORE_EXPORT
gint                    hyscan_location_get_progress            (HyScanLocation *location);

HYSCAN_CORE_EXPORT
HyScanLocation         *hyscan_location_new                     (HyScanDB       *db,
                                                                 HyScanCache    *cache,
                                                                 gchar          *cache_prefix,
                                                                 gchar          *track,
                                                                 gchar          *project,
                                                                 gdouble         quality);

G_END_DECLS

#endif /* __HYSCAN_LOCATION_H__ */

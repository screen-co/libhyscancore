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
 * В каждом галсе есть каналы данных: эхолоты, ГБО, GPS/GLONASS-приемники, САД. <BR>
 * Каждый КД дает определенный список параметров, которые из него можно извлечь, например, из RMC-строк можно получить информацию
 * о дате, времени, координатах, <BR>курсе и скорости. Эти параметры определены в структуре #HyScanLocationParameters.
 *
 * При работе с каналами данных и параметрами, класс оперирует понятием источник.<BR>
 * Источник - это некий номер, позволяющий однозначно сопоставить параметр обработки и канал данных.
 * Класс хранит информацию об источниках в таблице HyScanLocationSourcesList. Это внутренняя таблица, повлиять на которую извне невозможно,
 * однако можно получить <BR>список источников для параметра навигационных данных с помощью функции #hyscan_location_source_list.
 * Так же можно получить индекс активного источника для параметра обработки с помощью функции #hyscan_location_source_get.
 * Установка источника производится <BR>с помощью функции #hyscan_location_source_set.
 *
 * Настройка работы алгоритмов заключается в установке параметра quality (либо при создании объекта, либо во время работы с помощью функции #hyscan_location_quality_set).
 * При меньшем значении этого параметра данные сглаживаются сильней. Возможный диапазон значений - от 0.0 до 1.0.
 *
 * Работа класса разделена на три этапа. <BR>
 * На первом этапе объект создается (с помощью функций #hyscan_location_new, #hyscan_location_new_with_cache,
 * #hyscan_location_new_with_cache_prefix)  считывается список КД в галсе, составляется список источников,
 * устанавливаются источники по умолчанию, инициализируются локальные кэши, запускается поток-надзиратель.
 * Также при создании открывается группа параметров пользовательской обработки, которая определяется названием галса и префиксом.
 *
 * Работа потока-надзирателя - это второй этап работы класса.
 * Этот поток постоянно проверяет БД на предмет наличия новых данных, разбирает их и складывает
 * в локальные кэши (для каждого параметра свой кэш). При этом может производиться предобработка данных,
 * например, координаты сглаживаются кривыми Безье. Так же этот поток следит за пользовательскими параметрами обработки.
 *
 * Третий этап - это получение информации о местоположении судна. Для этого пользователем вызывается метод #hyscan_location_get.
 * В этом методе последовательно вызываются функции-getter'ы для каждого параметра,
 * который интересует пользователя. Getter'ы аппроксимируют данные (если требуется) и выдают их наверх.
 *
 * Объект поддерживает работу с т.н. пользовательскими параметрами обработки. Схемы данных этих параметров описаны в файле location-schema.xml.
 * Название <I>группы параметров</I> должно быть либо передано на этапе создания объекта, либо установлено в "location.default.[track]",
 * где [track] - название галса.
 * Названия <I>объектов</I> должны начинаться с префикса, соответствующего идентификатору (location-edit-latlong, location-edit-track,
 * location-edit-roll, location-edit-pitch, location-bulk-edit, location-remove, location-bulk-remove).
 *
 * Публично доступны следующие методы:
 * - #hyscan_location_new - создание объекта;
 * - #hyscan_location_new_with_cache - создание объекта с использованием системы кэширования;
 * - #hyscan_location_new_with_cache_prefix - создание объекта с использованием системы кэширования и префиксом;
 * - #hyscan_location_source_list - список доступных источников для заданного параметра;
 * - #hyscan_location_source_list_free - освобождает память, занятую списком источников;
 * - #hyscan_location_source_get - выбранный источник для заданного параметра;
 * - #hyscan_location_source_set - установка источника для заданного параметра;
 * - #hyscan_location_soundspeed_set - установка профиля скорости звука;
 * - #hyscan_location_get - получение информации о местоположении;
 * - #hyscan_location_get_mod_count - счетчик изменений;
 * - #hyscan_location_get_progress - процент выполнения расчетов (режим пост-обработки)
 *
 * - - -
 * Поддерживаемые схемы данных:
 * - location-edit-latlong - правка координат в точке;
 * - location-edit-track - правка курса для одной или нескольких точек;
 * - location-edit-roll - правка крена для одной или нескольких точек;
 * - location-edit-pitch - правка дифферента для одной или нескольких точек;
 * - location-bulk-remove - удаление одной или нескольких точек из обработки.
 *
 * - - -
 * Дополнительная информация о таблице скорости звука.
 *
 *  Под профилем скорости звука понимается таблично заданная функция скорости звука от глубины, например, такая:
 * | Глубина, м | Скорость звука, м/с |
 * |:----------:|:-------------------:|
 * |      0     |         1500        |
 * |      2     |         1450        |
 * |      4     |         1400        |
 * Где глубина задается в метрах, а скорость звука в метрах в секунду. <br>
 * Данный пример говорит о следующем: на глубинах от 0 до 2 метров скорость звука составляет 1500 м/с,
 * от 2 до 4 метров - 1450 м/с, от 4 и далее - 1400 м/с. <br>
 * \warning Глубина обязательно должна начинаться от нуля. <br>
 * Таблица представляет собой GArray, состоящий из структур типа SoundSpeedTable.
 *
 */
#ifndef __HYSCAN_LOCATION_H__
#define __HYSCAN_LOCATION_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>
#include <hyscan-data-channel.h>
#include <hyscan-geo.h>

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
  HYSCAN_LOCATION_SOURCE_ZERO,                          /**< Нулевой источник. Всегда выдает нулевые значения. */
  HYSCAN_LOCATION_SOURCE_NMEA,                          /**< NMEA-строки. При этом информация непосредственно берется из строки. */
  HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED,                 /**< NMEA-строки, но значение вычисляется из других значений (курс и скорость из координат). */
  HYSCAN_LOCATION_SOURCE_ECHOSOUNDER,                   /**< Эхолот. */
  HYSCAN_LOCATION_SOURCE_SONAR_PORT,                    /**< ГБО, левый борт. */
  HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD,               /**< ГБО, правый борт. */
  HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT,              /**< ГБО, высокое разрешение, левый борт. */
  HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD,         /**< ГБО, высокое разрешение, правый борт. */
  HYSCAN_LOCATION_SOURCE_SAS
} HyScanLocationSourceTypes;

/** \brief Таблица источников данных, отдаваемая при вызове  #hyscan_location_source_list */
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
  HyScanGeo parent_instance;

  HyScanLocationPrivate *priv;
};

struct _HyScanLocationClass
{
  HyScanGeoClass parent_class;
};

GType                   hyscan_location_get_type                (void);


/**
 *
 * Функция создает новый объект обработки навигационных данных.
 *
 * \param db - указатель на базу данных;
 * \param project - название проекта;
 * \param track - название галса;
 * \param param_prefix - префикс группы параметров;
 * \param quality - качество данных.
 *
 * \return location указатель на объект обработки навигационных данных.
 *
 */
HYSCAN_CORE_EXPORT
HyScanLocation         *hyscan_location_new                     (HyScanDB       *db,
                                                                 gchar          *project,
                                                                 gchar          *track,
                                                                 gchar          *param_prefix,
                                                                 gdouble         quality);

/**
 *
 * Функция создает новый объект обработки навигационных данных
 * с использованием системы кэширования.
 *
 * \param db - указатель на базу данных;
 * \param cache - указатель на объект системы кэширования;
 * \param project - название проекта;
 * \param track - название галса;
 * \param param_prefix - префикс группы параметров;
 * \param quality - качество данных.
 *
 * \return location указатель на объект обработки навигационных данных.
 *
 */
HYSCAN_CORE_EXPORT
HyScanLocation         *hyscan_location_new_with_cache          (HyScanDB       *db,
                                                                 HyScanCache    *cache,
                                                                 gchar          *project,
                                                                 gchar          *track,
                                                                 gchar          *param_prefix,
                                                                 gdouble         quality);

/**
 *
 * Функция создает новый объект обработки навигационных данных
 * с использованием системы кэширования и префиксом.
 *
 * \param db - указатель на базу данных;
 * \param cache - указатель на объект системы кэширования;
 * \param cache_prefix - префикс для системы кэширования;
 * \param project - название проекта;
 * \param track - название галса;
 * \param param_prefix - префикс группы параметров;
 * \param quality - качество данных.
 *
 * \return location указатель на объект обработки навигационных данных.
 *
 */
HYSCAN_CORE_EXPORT
HyScanLocation         *hyscan_location_new_with_cache_prefix   (HyScanDB       *db,
                                                                 HyScanCache    *cache,
                                                                 gchar          *cache_prefix,
                                                                 gchar          *project,
                                                                 gchar          *track,
                                                                 gchar          *param_prefix,
                                                                 gdouble         quality);

/**
 *
 * Функция позволяет задать период работы потока-надзирателя.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param overseer_period период работы потока-надзирателя в микросекундах.
 *
 */

HYSCAN_CORE_EXPORT
void                    hyscan_location_overseer_period_set     (HyScanLocation  *location,
                                                                 gint32           overseer_period);

/**
 *
 * Функция позволяет задать качество данных.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param quality качество данных.
 *
 */

HYSCAN_CORE_EXPORT
void                    hyscan_location_quality_set             (HyScanLocation  *location,
                                                                 gdouble          quality);
/**
 *
 * Функция возвращает нуль-терминированный список источников для заданного параметра.
 *
 * После использования нужно освободить занятую списком память с помощью функции #hyscan_location_source_list_free.
 * На последнем месте в списке источников находится структура со всеми полями, установленными в 0.
 * Это признак конца списка источников.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param parameter параметр из #HyScanLocationParameters.
 *
 * \return указатель на массив со списком источников.
 *
 */
HYSCAN_CORE_EXPORT
HyScanLocationSources **hyscan_location_source_list             (HyScanLocation *location,
                                                                 gint            parameter);

/**
 *
 * Функция освобождает память, занятую списоком источников для заданного параметра.
 *
 * Это удобный механизм освобождения памяти, занятой списком источников.
 * Однако пользователь может вручную освободить эту область памяти.
 *
 * \param data указатель на список источников.
 *
 */

HYSCAN_CORE_EXPORT
void                    hyscan_location_source_list_free        (HyScanLocationSources ***data);

/**
 *
 * Функция возвращает активный источник для заданного параметра.
 *
 * \param location указатель на объект обработки навигационных данных
 * \param parameter параметр из #HyScanLocationParameters.
 *
 * \return индекс активного источника. -1 сигнализирует о том,
 * что либо параметр исключен из обработки, либо есть проблемы с БД.
 *
 */
HYSCAN_CORE_EXPORT
gint                    hyscan_location_source_get              (HyScanLocation *location,
                                                                 gint            parameter);

/**
 *
 * Функция устанавливает источник.
 *
 * При вызове #hyscan_location_source_list возвращается список источников,
 * в котором каждому источнику присвоен индекс.
 * Этот индекс передается в hyscan_location_source_set, которая автоматически определяет,
 * для какого параметра актуален этот источник.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param source индекс источника данных;
 * \param turn_on если FALSE, то параметр, соответствующий этому источнику, исключается из обработки.
 *
 * \return TRUE, если источник корректно установился.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                hyscan_location_source_set              (HyScanLocation *location,
                                                                 gint            source,
                                                                 gboolean        turn_on);


/**
 *
 * Функция устанавливает таблицу профиля скорости звука.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param soundspeedtable таблица скорости звука #SoundSpeedTable.
 *
 * \return FALSE, если база данных недоступна, TRUE во всех остальных случаях.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                hyscan_location_soundspeed_set          (HyScanLocation *location,
                                                                 GArray          soundspeedtable);

/**
 *
 * Функция получения координат в заданный момент времени.
 *
 * В выходной структуре для выбранных параметров будут указаны значения, для невыбранных - NAN.
 * Возможна ситуация, при которой для заданного момента времени нет данных
 * (либо не существуют, либо еще не обработаны).
 * В таком случае будут возвращены все собранные данные, но те данные, что собрать не удалось,
 * будут установлены в NAN. Дополнительно флаг validity будет установлен в FALSE.
 *
 * \param location указатель на объект обработки навигационных данных;
 * \param parameter параметр из #HyScanLocationParameters. Параметры можно объединять логическим ИЛИ;
 * \param time время;
 * \param x сдвижка по оси от центра масс к носу;
 * \param y сдвижка по оси от центра масс к правому борту;
 * \param z сдвижка по оси от центра масс к килю;
 * \param psi угловой сдвиг (в радианах), соответствующий курсу;
 * \param gamma угловой сдвиг (в радианах), соответствующий крену;
 * \param theta угловой сдвиг (в радианах), соответствующий дифференту;
 * \param topo требовать координаты в топоцентрической СК.
 *
 * \return Структура #HyScanLocationData со всей затребованной информацией.
 *
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
                                                                 gdouble         theta,
                                                                 gboolean        topo);

/**
 *
 * Функция возвращает диапазон индексов, в которых есть данные широты и долготы.
 * Вызывается перед #hyscan_location_get_raw_data.
 *
 * \param location - указатель на объект обработки навигационных данных;
 * \param lindex - левый индекс данных;
 * \param rindex - правый индекс данных.
 *
 * \return TRUE, если диапазон успешно определен.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                hyscan_location_get_range               (HyScanLocation *location,
                                                                 gint32         *lindex,
                                                                 gint32         *rindex);

/**
 *
 * Функция возвращает сырые координаты для указанного индекса БД.
 * Вызывается перед #hyscan_location_get_raw_data.
 *
 * \warning Перед вызовом этой функции необходимо определить диапазон индексов, в которых
 * есть данные координат, с помощью функции #hyscan_location_get_range.
 *
 * \param location - указатель на объект обработки навигационных данных;
 * \param index - индекс данных;
 *
 * \return Значение, хранящееся в БД по этому индексу. При этом если данные в строке невалидны
 * (не совпадает контрольная сумма), то поля будут иметь значение NAN, а флаг валидности будет равен HYSCAN_LOCATION_INVALID.
 * В случае валидных данных флаг будет HYSCAN_LOCATION_PARSED.
 *
 */
HYSCAN_CORE_EXPORT
HyScanLocationData      hyscan_location_get_raw_data            (HyScanLocation *location,
                                                                 gint32          index);
/**
 *
 * Функция возвращает номер изменения в объекте.
 * Номер изменения увеличивается в следующих случаях: установка нового источника данных,
 * установка таблицы скорости звука, установка нового значения quality, изменения в группе пользовательских параметров.
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
gint64                  hyscan_location_get_mod_count           (HyScanLocation *location);
HYSCAN_CORE_EXPORT
gint                    hyscan_location_get_progress            (HyScanLocation *location);

G_END_DECLS

#endif /* __HYSCAN_LOCATION_H__ */

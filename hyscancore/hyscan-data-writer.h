/**
 * \file hyscan-data-writer.h
 *
 * \brief Заголовочный файл класса управления записью данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDataWriter HyScanDataWriter - класс управления записью данных
 *
 * Класс предназначен для управления записью данных в систему хранения.
 *
 * Для управления записью предназначены функции:
 *
 * - #hyscan_data_writer_start - включает запись данных;
 * - #hyscan_data_writer_stop - останавливает запись данных.
 *
 * При вызове функции #hyscan_data_writer_start создаётся новый галс, в который ничаниется запись
 * данных. Эту функцию можно вызывать если запись уже включена. В этом случае произойдёт переключение
 * записываемого галса.
 *
 * Для установки параметров записи предназначены функции:
 *
 * - #hyscan_data_writer_set_chunk_size - устанавливает максимальный размер файлов в галсе;
 * - #hyscan_data_writer_set_save_time - интервал времени, для которого сохраняются записываемые данные;
 * - #hyscan_data_writer_set_save_size - задаёт объём сохраняемых данных в канале.
 *
 * Значения, установленные этими функциями, применяются к текущему записываемому галсу и ко всем последующим.
 * Для того, что бы отменить действие установленных значений необходимо установить новые значения или
 * передать отрицательное число. В этом случае будет установленно значение по умолчанию.
 *
 * Значения по умолчанию будут применяться только к вновь создаваемым каналам данных.
 *
 * Для записи данных и вспомогательной информации используются следующие функции:
 *
 * - #hyscan_data_writer_sensor_set_position - для установки местоположения приёмной антенны датчика;
 * - #hyscan_data_writer_sensor_add_data - для записи данных от датчика;
 * - #hyscan_data_writer_acoustic_set_position - для установки местоположения приёмной антенны гидролокатора;
 * - #hyscan_data_writer_acoustic_add_data - для записи акустических данных от гидролокатора.
 *
 * "Сырые" акустические данные могут содержать вспомогательную информацию: образы сигналов для свёртки
 * и параметры системы ВАРУ. Для записи этой информации предназначены функции:
 *
 * - #hyscan_data_writer_acoustic_add_signal - для записи образа сигнала;
 * - #hyscan_data_writer_acoustic_add_tvg - для записи параметров ВАРУ.
 *
 * Класс HyScanDataWriter сохраняет во внутреннем буфере текущий образ сигнала и параметры ВАРУ
 * для каждого из источников данных. В дальнейшем эта информация записывается в новые галсы автоматически,
 * до тех пор пока не будут установлены новые значения.
 *
 */

#ifndef __HYSCAN_DATA_WRITER_H__
#define __HYSCAN_DATA_WRITER_H__

#include <hyscan-core-types.h>

G_BEGIN_DECLS

/** \brief Данные от гидролокатора и датчиков */
typedef struct
{
  gint64                       time;                           /**< Время приёма данных. */
  guint32                      size;                           /**< Размер данных. */
  gconstpointer                data;                           /**< Данные. */
} HyScanDataWriterData;

/** \brief Образ излучаемого сигнала для свёртки */
typedef struct
{
  gint64                       time;                           /**< Время начала действия сигнала. */
  gfloat                       rate;                           /**< Частота дискретизации, Гц. */
  guint32                      n_points;                       /**< Число точек образа. */
  const HyScanComplexFloat    *points;                         /**< Образ сигнала для свёртки. */
} HyScanDataWriterSignal;

/** \brief Значения параметров усиления системы ВАРУ */
typedef struct
{
  gint64                       time;                           /**< Время начала действия параметров системы ВАРУ. */
  gfloat                       rate;                           /**< Частота дискретизации, Гц. */
  guint32                      n_gains;                        /**< Число коэффициентов передачи. */
  const gfloat                *gains;                          /**< Коэффициенты передачи приёмного тракта, дБ. */
} HyScanDataWriterTVG;

#define HYSCAN_TYPE_DATA_WRITER             (hyscan_data_writer_get_type ())
#define HYSCAN_DATA_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DATA_WRITER, HyScanDataWriter))
#define HYSCAN_IS_DATA_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DATA_WRITER))
#define HYSCAN_DATA_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DATA_WRITER, HyScanDataWriterClass))
#define HYSCAN_IS_DATA_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DATA_WRITER))
#define HYSCAN_DATA_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DATA_WRITER, HyScanDataWriterClass))

typedef struct _HyScanDataWriter HyScanDataWriter;
typedef struct _HyScanDataWriterPrivate HyScanDataWriterPrivate;
typedef struct _HyScanDataWriterClass HyScanDataWriterClass;

struct _HyScanDataWriter
{
  GObject parent_instance;

  HyScanDataWriterPrivate *priv;
};

struct _HyScanDataWriterClass
{
  GObjectClass parent_class;
};

GType                  hyscan_data_writer_get_type                     (void);

/**
 *
 * Функция создаёт новый объект \link HyScanDataWriter \endlink.
 *
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return Указатель на объект \link HyScanDataWriter \endlink.
 *
 */
HyScanDataWriter      *hyscan_data_writer_new                          (HyScanDB                      *db);

/**
 *
 * Функция включает запись данных.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param project_name название проекта, в который записывать данные;
 * \param track_name название галса, в который записывать данные;
 * \param track_type тип галса.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_start                        (HyScanDataWriter              *writer,
                                                                        const gchar                   *project_name,
                                                                        const gchar                   *track_name,
                                                                        HyScanTrackType                track_type);

/**
 *
 * Функция отключает запись данных.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink.
 *
 * \return Нет.
 *
 */
void                   hyscan_data_writer_stop                         (HyScanDataWriter              *writer);

/**
 *
 * Функция устанавливает максимальный размер файлов в галсе. Подробнее
 * об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param chunk_size максимальный размер файлов в байтах или отрицательное число.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_set_chunk_size               (HyScanDataWriter              *writer,
                                                                        gint32                         chunk_size);

/**
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые данные. Если данные
 * были записаны ранее "текущего времени" - "интервал хранения" они удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param save_time время хранения данных в микросекундах или отрицательное число.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_set_save_time                (HyScanDataWriter              *writer,
                                                                        gint64                         save_time);

/**
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём данных превышает этот предел,
 * старые данные удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param save_size объём сохраняемых данных в байтах или отрицательное число.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_set_save_size                (HyScanDataWriter              *writer,
                                                                        gint64                         save_size);

/**
 *
 * Функция устанавливает информацию о местоположении приёмной антенны датчика.
 * Эта информация будет записываться в канал базы данных для всех датчиков,
 * для которых, приём данных осуществляется через порт с указанным именем.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param sensor название порта датчика;
 * \param position информация о местоположении приёмной антенны - \link HyScanAntennaPosition \endlink.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_sensor_set_position          (HyScanDataWriter              *writer,
                                                                        const gchar                   *sensor,
                                                                        HyScanAntennaPosition         *position);

/**
 *
 * Функция записывает данные от датчиков.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param sensor название порта датчика;
 * \param source тип источника данных;
 * \param channel индекс канала данных, начиная с 1;
 * \param data данные датчика.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_sensor_add_data              (HyScanDataWriter              *writer,
                                                                        const gchar                   *sensor,
                                                                        HyScanSourceType               source,
                                                                        guint                          channel,
                                                                        HyScanDataWriterData          *data);

/**
 *
 * Функция устанавливает информацию о местоположении приёмной антенны гидролокатора.
 * Эта информация записывается в базу данных для всех гидролокационных данных с указанным
 * типом источника данных и производных от него.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param source тип источника данных;
 * \param position информация о местоположении приёмной антенны - \link HyScanAntennaPosition \endlink.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_acoustic_set_position        (HyScanDataWriter              *writer,
                                                                        HyScanSourceType               source,
                                                                        HyScanAntennaPosition         *position);

/**
 *
 * Функция записывает акустические данные.
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param source тип источника данных;
 * \param raw признак "сырых" данных;
 * \param channel индекс канала данных, начиная с 1;
 * \param info параметры акустических данных;
 * \param data акустические данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_acoustic_add_data            (HyScanDataWriter              *writer,
                                                                        HyScanSourceType               source,
                                                                        gboolean                       raw,
                                                                        guint                          channel,
                                                                        HyScanAcousticDataInfo        *info,
                                                                        HyScanDataWriterData          *data);

/**
 *
 * Функция устанавливает образ сигнала для свёртки для указанного источника данных.
 * Эта информация записывает в базу данных для "сырых" гидролокационных данных с указанным
 * типом источника данных
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param source тип источника данных;
 * \param signal образ сигнала.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_acoustic_add_signal          (HyScanDataWriter              *writer,
                                                                        HyScanSourceType               source,
                                                                        HyScanDataWriterSignal        *signal);

/**
 *
 * Функция устанавливает значения параметров усиления ВАРУ для указанного источника данных.
 * Эта информация записывает в базу данных для "сырых" гидролокационных данных с указанным
 * типом источника данных
 *
 * \param writer указатель на объект \link HyScanDataWriter \endlink;
 * \param source тип источника данных;
 * \param tvg параметры усиления ВАРУ.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
gboolean               hyscan_data_writer_acoustic_add_tvg             (HyScanDataWriter              *writer,
                                                                        HyScanSourceType               source,
                                                                        HyScanDataWriterTVG           *tvg);

G_END_DECLS

#endif /* __HYSCAN_DATA_WRITER_H__ */

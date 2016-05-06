/**
 * \file hyscan-data-channel-writer.h
 *
 * \brief Заголовочный файл класса записи акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDataChannelWriter HyScanDataChannelWriter - класс записи акустических данных
 *
 * Класс HyScanDataChannelWriter используется для создания и записи каналов с акустическими данными.
 *
 * Для создания объекта записи данных используется функция #hyscan_data_channel_writer_new. При
 * создании канала данных необходимо передать его параметры, определяемые структурой
 * \link HyScanDataChannelInfo \endlink.
 *
 * Функции #hyscan_data_channel_writer_set_chunk_size, #hyscan_data_channel_writer_set_save_time
 * и #hyscan_data_channel_writer_set_save_size используются для управления записью. Их назначение
 * аналогично функциям \link hyscan_db_channel_set_chunk_size \endlink,
 * \link hyscan_db_channel_set_save_time \endlink и \link hyscan_db_channel_set_save_size \endlink
 * в интерфейсе \link HyScanDB \endlink.
 *
 * Для задания образца сигнала, используемого для свёртки данных, предназначена функция
 * #hyscan_data_channel_writer_add_signal_image. При определении образца указывается момент
 * времени, с которого применяется данный образец. Последующий вызовы этой функции будут
 * приводить к добавлению новых образцов. Таким образом свертка данных будет проводится
 * с разными образцами, выбор которых осуществляется по времени. Задание пустого образца
 * (image=NULL и n_points=0) приведёт к отключению свёртки с указанного момента
 * времени. Повторное задание образца вновь включит свёртку.
 *
 * Для записи данных используется функция #hyscan_data_channel_writer_add_data.
 *
 */

#ifndef __HYSCAN_DATA_CHANNEL_WRITER_H__
#define __HYSCAN_DATA_CHANNEL_WRITER_H__

#include <hyscan-db.h>
#include <hyscan-data-channel-common.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DATA_CHANNEL_WRITER             (hyscan_data_channel_writer_get_type ())
#define HYSCAN_DATA_CHANNEL_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DATA_CHANNEL_WRITER, HyScanDataChannelWriter))
#define HYSCAN_IS_DATA_CHANNEL_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DATA_CHANNEL_WRITER))
#define HYSCAN_DATA_CHANNEL_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DATA_CHANNEL_WRITER, HyScanDataChannelWriterClass))
#define HYSCAN_IS_DATA_CHANNEL_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DATA_CHANNEL_WRITER))
#define HYSCAN_DATA_CHANNEL_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DATA_CHANNEL_WRITER, HyScanDataChannelWriterClass))

typedef struct _HyScanDataChannelWriter HyScanDataChannelWriter;
typedef struct _HyScanDataChannelWriterPrivate HyScanDataChannelWriterPrivate;
typedef struct _HyScanDataChannelWriterClass HyScanDataChannelWriterClass;

struct _HyScanDataChannelWriter
{
  GObject parent_instance;

  HyScanDataChannelWriterPrivate *priv;
};

struct _HyScanDataChannelWriterClass
{
  GObjectClass parent_class;
};

HYSCAN_CORE_EXPORT
GType hyscan_data_channel_writer_get_type (void);

/**
 *
 * Функция создаёт новый канал акустических данных и открывает его для записи.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 * \param channel_info параметры канала данных (\link HyScanDataChannelInfo \endlink).
 *
 * \return Указатель на объект \link HyScanDataChannelWriter \endlink.
 *
 */
HYSCAN_CORE_EXPORT
HyScanDataChannelWriter       *hyscan_data_channel_writer_new                  (HyScanDB                  *db,
                                                                                const gchar               *project_name,
                                                                                const gchar               *track_name,
                                                                                const gchar               *channel_name,
                                                                                HyScanDataChannelInfo     *channel_info);

/**
 *
 * Функция задаёт максимальный размер файлов, хранящих данные канала.
 *
 * \param dwriter указатель на объект \link HyScanDataChannelWriter \endlink;
 * \param chunk_size максимальный размер файла в байтах.
 *
 * \return TRUE - если максимальный размер файлов изменён, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                       hyscan_data_channel_writer_set_chunk_size       (HyScanDataChannelWriter   *dwriter,
                                                                                gint32                     chunk_size);

/**
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые данные.
 *
 * \param dwriter указатель на объект \link HyScanDataChannelWriter \endlink;
 * \param save_time время хранения данных в микросекундах.
 *
 * \return TRUE - если время хранения данных изменено, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                       hyscan_data_channel_writer_set_save_time        (HyScanDataChannelWriter   *dwriter,
                                                                                gint64                     save_time);

/**
 *
 * Функция задаёт объём сохраняемых данных в канале.
 *
 * \param dwriter указатель на объект \link HyScanDataChannelWriter \endlink;
 * \param save_size объём сохраняемых данных в байтах.
 *
 * \return TRUE - если объём сохраняемых данных изменён, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                       hyscan_data_channel_writer_set_save_size        (HyScanDataChannelWriter   *dwriter,
                                                                                gint64                     save_size);

/**
 *
 * Функция задаёт образец сигнала для свёртки.
 *
 * \param dwriter указатель на объект \link HyScanDataChannelWriter \endlink;
 * \param time метка времени начала действия образца сигнала;
 * \param image образец сигнала для свёртки;
 * \param n_points число точек в образце сигнала.
 *
 * \return TRUE - если образец сигнала был установлен, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                       hyscan_data_channel_writer_add_signal_image     (HyScanDataChannelWriter   *dwriter,
                                                                                gint64                     time,
                                                                                HyScanComplexFloat        *image,
                                                                                gint32                     n_points);

/**
 *
 * Функция записывает новые данные в канал.
 *
 * \param dwriter указатель на объект \link HyScanDataChannelWriter \endlink;
 * \param time метка времени в микросекундах;
 * \param data указатель на записываемые данные;
 * \param size размер записываемых данных.
 *
 * \return TRUE - если данные успешно записаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean                       hyscan_data_channel_writer_add_data             (HyScanDataChannelWriter   *dwriter,
                                                                                gint64                     time,
                                                                                gpointer                   data,
                                                                                gint32                     size);

G_END_DECLS

#endif /* __HYSCAN_DATA_CHANNEL_WRITER_H__ */

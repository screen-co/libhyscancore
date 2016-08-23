/**
 * \file hyscan-data-channel.h
 *
 * \brief Заголовочный файл класса обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDataChannel HyScanDataChannel - класс обработки акустических данных
 *
 * Класс HyScanDataChannel используется для обработки акустических данных, такой как преобразование
 * типов данных, свёртки, вычисления аплитуды сигнала.
 *
 * В своей работе класс HyScanDataChannel может использовать внешний кэш для хранения обработанных
 * данных. В случае запроса ранее обработанных данных пользователь получит их копию из кэша. Несколько
 * экземпляров класса HyScanDataChannel, обрабатывающих одинаковые каналы данных могут использовать
 * один и тот же кэш. Таким образом, данные обработанные в одной части приложения не потребуют повторной
 * обработки в другой его части.
 *
 * Местоположение приёмной антенны гидролокатора можно получить функцией #hyscan_data_channel_get_position.
 * Параметры акустических данных можно получить функцией #hyscan_data_channel_get_info.
 *
 * Функции #hyscan_data_channel_get_range и #hyscan_data_channel_find_data используются для определения
 * границ записанных данных и их поиска по метке времени. Эти функции аналогичны функциям
 * \link hyscan_db_channel_get_data_range \endlink и \link hyscan_db_channel_find_data \endlink интерфейса
 * \link HyScanDB \endlink.
 *
 * Число точек данных, для указанного индекса, можно получить функцией #hyscan_data_channel_get_values_count.
 *
 * Время приёма данных, для указанного индекса, можно получить функцией #hyscan_data_channel_get_time.
 *
 * Для чтения данных используются следующие функции:
 *
 * - #hyscan_data_channel_get_amplitude_values - функция возвращает амплитуду акустического сигнала;
 * - #hyscan_data_channel_get_quadrature_values - функция возвращает квадратурные отсчёты акустического сигнала.
 *
 * Функции чтения данных, используют в работе параметр buffer_size, определяющий размер буфера для
 * данных. Этот размер определяет число точек данных, способных поместиться в буфере.
 *
 * Функции  #hyscan_data_channel_get_amplitude_values и #hyscan_data_channel_get_quadrature_values могут
 * проводить обработку как с выполненим свёртки данных с образцом сигнала, так и без неё. Образцы сигналов для
 * свёртки хранятся вместе с данными. Имеется возможность временно отключить свёртку или включить её снова
 * функцией #hyscan_data_channel_set_convolve.
 *
 * После выполнения первичной обработки, данные возвращаются в формате gfloat или \link HyScanComplexFloat \endlink.
 * Амплитудные значения находятся в пределах от нуля до единицы, квадратурные от минус единицы до единицы.
 *
 * Объекты обработки данных создаются функциями #hyscan_data_channel_new, #hyscan_data_channel_new_with_cache и
 * #hyscan_data_channel_new_with_cache_prefix.
 *
 * HyScanDataChannel не поддерживает работу в многопоточном режиме. Рекомендуется создавать свой экземпляр
 * объекта обработки данных в каждом потоке и использовать единый кэш данных.
 *
 */

#ifndef __HYSCAN_DATA_CHANNEL_H__
#define __HYSCAN_DATA_CHANNEL_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DATA_CHANNEL             (hyscan_data_channel_get_type ())
#define HYSCAN_DATA_CHANNEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannel))
#define HYSCAN_IS_DATA_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DATA_CHANNEL))
#define HYSCAN_DATA_CHANNEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannelClass))
#define HYSCAN_IS_DATA_CHANNEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DATA_CHANNEL))
#define HYSCAN_DATA_CHANNEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannelClass))

typedef struct _HyScanDataChannel HyScanDataChannel;
typedef struct _HyScanDataChannelPrivate HyScanDataChannelPrivate;
typedef struct _HyScanDataChannelClass HyScanDataChannelClass;

struct _HyScanDataChannel
{
  GObject parent_instance;

  HyScanDataChannelPrivate *priv;
};

struct _HyScanDataChannelClass
{
  GObjectClass parent_class;
};

HYSCAN_CORE_EXPORT
GType hyscan_data_channel_get_type (void);

/**
 *
 * Функция создаёт новый объект обработки акустических данных без использования кэша.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных.
 *
 * \return Указатель на объект \link HyScanDataChannel \endlink.
 *
 */
HYSCAN_CORE_EXPORT
HyScanDataChannel     *hyscan_data_channel_new                     (HyScanDB              *db,
                                                                    const gchar           *project_name,
                                                                    const gchar           *track_name,
                                                                    const gchar           *channel_name);

/**
 *
 * Функция создаёт новый объект обработки акустических данных с использованием кэша.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 * \param cache указатель на интерфейс \link HyScanCache \endlink.
 *
 * \return Указатель на объект \link HyScanDataChannel \endlink.
 *
 */

HYSCAN_CORE_EXPORT
HyScanDataChannel     *hyscan_data_channel_new_with_cache          (HyScanDB              *db,
                                                                    const gchar           *project_name,
                                                                    const gchar           *track_name,
                                                                    const gchar           *channel_name,
                                                                    HyScanCache           *cache);


/**
 *
 * Функция создаёт новый объект обработки акустических данных с использованием кэша и префикса.
 * Префикс используется как вспомогательный признак при кэшировании данных.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param cache_prefix префикс ключа кэширования.
 *
 * \return Указатель на объект \link HyScanDataChannel \endlink.
 *
 */
HYSCAN_CORE_EXPORT
HyScanDataChannel     *hyscan_data_channel_new_with_cache_prefix   (HyScanDB              *db,
                                                                    const gchar           *project_name,
                                                                    const gchar           *track_name,
                                                                    const gchar           *channel_name,
                                                                    HyScanCache           *cache,
                                                                    const gchar           *cache_prefix);

/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны гидролокатора
 * в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink.
 *
 * \return Местоположение приёмной антенны гидролокатора.
 *
 */
HYSCAN_CORE_EXPORT
HyScanAntennaPosition  hyscan_data_channel_get_position            (HyScanDataChannel *dchannel);

/**
 *
 * Функция возвращает параметры канала акустических данных в виде
 * значения структуры \link HyScanAcousticDataInfo \endlink.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink.
 *
 * \return Параметры канала акустических данных.
 *
 */
HYSCAN_CORE_EXPORT
HyScanAcousticDataInfo hyscan_data_channel_get_info                (HyScanDataChannel *dchannel);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param first_index указатель на переменную для начального индекса или NULL;
 * \param last_index указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_data_channel_get_range               (HyScanDataChannel     *dchannel,
                                                                    gint32                *first_index,
                                                                    gint32                *last_index);

/**
 *
 * Функция возвращает число точек данных для указанного индекса.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс данных.
 *
 * \return Число точек для указанного индекса или отрицательное число в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gint32                 hyscan_data_channel_get_values_count        (HyScanDataChannel     *dchannel,
                                                                    gint32                 index);

/**
 *
 * Функция возвращает время приёма данных для указанного индекса.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс данных.
 *
 * \return Время приёма данных для указанного индекса или отрицательное число в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gint64                 hyscan_data_channel_get_time                (HyScanDataChannel     *dchannel,
                                                                    gint32                 index);

/**
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return TRUE - если данные найдены, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_data_channel_find_data               (HyScanDataChannel     *dchannel,
                                                                    gint64                 time,
                                                                    gint32                *lindex,
                                                                    gint32                *rindex,
                                                                    gint64                *ltime,
                                                                    gint64                *rtime);

/**
 *
 * Функция включает или отключает выполнения свёртки акустических данных.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param convolve выполнять (TRUE) или нет (FALSE) свёртку акустических данных.
 *
 * \return Нет.
 *
 */
HYSCAN_CORE_EXPORT
void                   hyscan_data_channel_set_convolve            (HyScanDataChannel     *dchannel,
                                                                    gboolean               convolve);

/**
 *
 * Функция возвращает значения амплитуды акустического сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер
 * считанных данных в точках. Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен gfloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_data_channel_get_amplitude_values    (HyScanDataChannel     *dchannel,
                                                                    gint32                 index,
                                                                    gfloat                *buffer,
                                                                    gint32                *buffer_size,
                                                                    gint64                *time);

/**
 *
 * Функция возвращает квадратурные отсчёты акустического сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер
 * считанных данных в точках. Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен HyScanComplexFloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_data_channel_get_quadrature_values   (HyScanDataChannel     *dchannel,
                                                                    gint32                 index,
                                                                    HyScanComplexFloat    *buffer,
                                                                    gint32                *buffer_size,
                                                                    gint64                *time);

G_END_DECLS

#endif /* __HYSCAN_DATA_CHANNEL_H__ */

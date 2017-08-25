/**
 * \file hyscan-acoustic-data.h
 *
 * \brief Заголовочный файл класса обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanAcousticData HyScanAcousticData - класс обработки акустических данных
 *
 * Класс HyScanAcousticData используется для получения амплитуды сигнала акустических данных.
 * В своей работе класс может использовать сырые или обработанные данные. Это указывается при
 * создании объекта функцией #hyscan_acoustic_data_new.
 *
 * В своей работе класс HyScanAcousticData может использовать внешний кэш для хранения обработанных
 * данных. В случае запроса ранее обработанных данных пользователь получит их копию из кэша. Несколько
 * экземпляров класса HyScanAcousticData, обрабатывающих одинаковые каналы данных могут использовать
 * один и тот же кэш. Таким образом, данные обработанные в одной части приложения не потребуют повторной
 * обработки в другой его части. Для задания кэша предназанчена функция #hyscan_acoustic_data_set_cache.
 *
 * Местоположение приёмной антенны гидролокатора можно получить функцией #hyscan_acoustic_data_get_position.
 * Параметры данных можно получить функцией #hyscan_acoustic_data_get_info.
 *
 * С помощью функции #hyscan_acoustic_data_get_source можно получить тип источника данных.
 *
 * Класс поддерживает обработку данных, запись которых производится в настоящий момент. В этом случае
 * могут появляться новые данные или исчезать уже записанные. Определить возможность изменения данных
 * можно с помощью функции #hyscan_acoustic_data_is_writable.
 *
 * Номер изменения в данных можно получить с помощью функции #hyscan_acoustic_data_get_mod_count.
 *
 * Функции #hyscan_acoustic_data_get_range и #hyscan_acoustic_data_find_data используются для определения
 * границ записанных данных и их поиска по метке времени. Эти функции аналогичны функциям
 * \link hyscan_db_channel_get_data_range \endlink и \link hyscan_db_channel_find_data \endlink интерфейса
 * \link HyScanDB \endlink.
 *
 * Для получения значений амплитуды сигнала используется функция #hyscan_acoustic_data_get_values.
 * Амплитудные значения находятся в пределах от нуля до единицы.
 *
 * HyScanAcousticData не поддерживает работу в многопоточном режиме. Рекомендуется создавать свой экземпляр
 * объекта обработки данных в каждом потоке и использовать единый кэш данных.
 *
 */

#ifndef __HYSCAN_ACOUSTIC_DATA_H__
#define __HYSCAN_ACOUSTIC_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_ACOUSTIC_DATA             (hyscan_acoustic_data_get_type ())
#define HYSCAN_ACOUSTIC_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_ACOUSTIC_DATA, HyScanAcousticData))
#define HYSCAN_IS_ACOUSTIC_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_ACOUSTIC_DATA))
#define HYSCAN_ACOUSTIC_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_ACOUSTIC_DATA, HyScanAcousticDataClass))
#define HYSCAN_IS_ACOUSTIC_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_ACOUSTIC_DATA))
#define HYSCAN_ACOUSTIC_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_ACOUSTIC_DATA, HyScanAcousticDataClass))

typedef struct _HyScanAcousticData HyScanAcousticData;
typedef struct _HyScanAcousticDataPrivate HyScanAcousticDataPrivate;
typedef struct _HyScanAcousticDataClass HyScanAcousticDataClass;

struct _HyScanAcousticData
{
  GObject parent_instance;

  HyScanAcousticDataPrivate *priv;
};

struct _HyScanAcousticDataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_acoustic_data_get_type           (void);

/**
 *
 * Функция создаёт новый объект обработки акустических данных. Если данные отсутствуют,
 * функция вернёт значение NULL.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param source_type тип источника данных;
 * \param raw использовать - TRUE или нет - FALSE сырые данные.
 *
 * \return Указатель на объект \link HyScanAcousticData \endlink или NULL.
 *
 */
HYSCAN_API
HyScanAcousticData    *hyscan_acoustic_data_new                (HyScanDB         *db,
                                                                const gchar      *project_name,
                                                                const gchar      *track_name,
                                                                HyScanSourceType  source_type,
                                                                gboolean          raw);

/**
 *
 * Функция задаёт используемый кэш и префикс идентификаторов объектов для
 * хранения в нём.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink или NULL;
 * \param prefix префикс ключа кэширования или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_acoustic_data_set_cache          (HyScanAcousticData            *data,
                                                                HyScanCache                   *cache,
                                                                const gchar                   *prefix);

/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны гидролокатора
 * в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink.
 *
 * \return Местоположение приёмной антенны гидролокатора.
 *
 */
HYSCAN_API
HyScanAntennaPosition  hyscan_acoustic_data_get_position       (HyScanAcousticData            *data);

/**
 *
 * Функция возвращает параметры канала акустических данных в виде значения
 * структуры \link HyScanAcousticDataInfo \endlink.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink.
 *
 * \return Параметры канала акустических данных.
 *
 */
HYSCAN_API
HyScanAcousticDataInfo hyscan_acoustic_data_get_info           (HyScanAcousticData            *data);

/**
 *
 * Функция возвращает тип источника данных.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink.
 *
 * \return Тип источника данных из \link HyScanSourceType \endlink.
 *
 */
HYSCAN_API
HyScanSourceType       hyscan_acoustic_data_get_source         (HyScanAcousticData            *data);

/**
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink.
 *
 * \return TRUE - если возможна запись данных, FALSE - если данные в режиме только чтения.
 *
 */
HYSCAN_API
gboolean               hyscan_acoustic_data_is_writable        (HyScanAcousticData            *data);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink;
 * \param first_index начальный индекс данных или NULL;
 * \param last_index конечный индекс данных или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_acoustic_data_get_range          (HyScanAcousticData            *data,
                                                                guint32                       *first_index,
                                                                guint32                       *last_index);

/**
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться на значение
 * номера изменения, важен только факт смены номера по сравнению с предыдущим запросом.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink.
 *
 * \return Номер изменения.
 *
 */
HYSCAN_API
guint32                hyscan_acoustic_data_get_mod_count      (HyScanAcousticData            *data);

/**
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink;
 * \param time искомый момент времени;
 * \param lindex "левый" индекс данных или NULL;
 * \param rindex "правый" индекс данных или NULL;
 * \param ltime "левая" метка времени данных или NULL;
 * \param rtime "правая" метка времени данных или NULL.
 *
 * \return TRUE - если данные найдены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
HyScanDBFindStatus     hyscan_acoustic_data_find_data          (HyScanAcousticData            *data,
                                                                gint64                         time,
                                                                guint32                       *lindex,
                                                                guint32                       *rindex,
                                                                gint64                        *ltime,
                                                                gint64                        *rtime);

/**
 *
 * Функция возвращает значения амплитуды сигнала.
 *
 *
 * \param data указатель на объект \link HyScanAcousticData \endlink;
 * \param index индекс считываемых данных;
 * \param n_points число точек данных;
 * \param time метка времени считанных данных или NULL.
 *
 * \return Значения амплитуд сигнала или NULL.
 *
 */
HYSCAN_API
const gfloat          *hyscan_acoustic_data_get_values         (HyScanAcousticData            *data,
                                                                guint32                        index,
                                                                guint32                       *n_points,
                                                                gint64                        *time);

G_END_DECLS

#endif /* __HYSCAN_ACOUSTIC_DATA_H__ */

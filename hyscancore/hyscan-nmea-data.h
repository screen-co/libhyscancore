/**
 * \file hyscan-nmea-data.h
 *
 * \brief Заголовочный файл класса обработки NMEA-строк.
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanNMEAData HyScanNMEAData - класс работы с NMEA-строками.
 *
 * \warning Класс не является потокобезопасным.
 *
 * Класс предназначен для работы с NMEA-строками. Он позволяет не открывать
 * каналы данных напрямую, а оперировать типами и номерами каналов
 * (См. \link HyScanSourceType \endlink). При этом класс не осуществляет верификацию строк,
 * однако это можно сделать вспомогательной функцией #hyscan_nmea_data_check_sentence.
 *
 * В канале HYSCAN_SOURCE_NMEA_ANY по одному индексу может оказаться несколько строк.
 * В таком случае класс вернет всю строку целиком. Для разбиения этой строки на отдельные
 * сообщения можно использовать функцию hyscan_nmea_data_split_sentence, которая вернет
 * нуль-терминированный список строк.
 *
 * - #hyscan_nmea_data_new - создает новый объект HyScanNMEAData;
 * - #hyscan_nmea_data_set_cache - устанавливает кэш;
 * - #hyscan_nmea_data_get_position - возвращает информацию о местоположении приемника;
 * - #hyscan_nmea_data_get_source - возвращает тип источника из \link HyScanSourceType \endlink;
 * - #hyscan_nmea_data_get_channel - возвращает номер канала;
 * - #hyscan_nmea_data_is_writable - определяет, возможна ли дозапись в канал;
 * - #hyscan_nmea_data_get_range - определяет диапазон данных в канале;
 * - #hyscan_nmea_data_find_data - ищет данные по времени;
 * - #hyscan_nmea_data_get_time - возвращает время приема строки;
 * - #hyscan_nmea_data_get_sentence - возвращает строку;
 * - #hyscan_nmea_data_check_sentence - проверка типа и контрольной суммы строки.
 * - #hyscan_nmea_data_split_sentence - разделение строк, содержащих несколько сообщений.
 *
 */

#ifndef __HYSCAN_NMEA_DATA_H__
#define __HYSCAN_NMEA_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_DATA             (hyscan_nmea_data_get_type ())
#define HYSCAN_NMEA_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_DATA, HyScanNMEAData))
#define HYSCAN_IS_NMEA_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_DATA))
#define HYSCAN_NMEA_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_DATA, HyScanNMEADataClass))
#define HYSCAN_IS_NMEA_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_DATA))
#define HYSCAN_NMEA_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_DATA, HyScanNMEADataClass))

typedef struct _HyScanNMEAData HyScanNMEAData;
typedef struct _HyScanNMEADataPrivate HyScanNMEADataPrivate;
typedef struct _HyScanNMEADataClass HyScanNMEADataClass;

struct _HyScanNMEAData
{
  GObject parent_instance;

  HyScanNMEADataPrivate *priv;
};

struct _HyScanNMEADataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_nmea_data_get_type               (void);

/**
 *
 * Функция создаёт новый объект обработки NMEA строк.
 * В требуемом канале данных должна быть хотя бы одна запись,
 * иначе будет возращен NULL.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param source_type тип источника данных;
 * \param source_channel индекс канала данных.
 *
 * \return Указатель на объект \link HyScanNMEAData \endlink или NULL.
 *
 */
HYSCAN_API
HyScanNMEAData         *hyscan_nmea_data_new                    (HyScanDB              *db,
                                                                 const gchar           *project_name,
                                                                 const gchar           *track_name,
                                                                 HyScanSourceType       source_type,
                                                                 guint                  source_channel);
/**
 *
 * Функция задаёт используемый кэш и префикс идентификаторов объектов для
 * хранения в нём.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink или NULL;
 *
 */
HYSCAN_API
void                    hyscan_nmea_data_set_cache              (HyScanNMEAData        *data,
                                                                 HyScanCache           *cache);
/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны.
 * Информация передается в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink.
 *
 * \return Местоположение приёмной антенны.
 *
 */
HYSCAN_API
HyScanAntennaPosition   hyscan_nmea_data_get_position           (HyScanNMEAData        *data);

/**
 *
 * Функция возвращает тип источника данных.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink.
 *
 * \return Тип источника данных из \link HyScanSourceType \endlink.
 *
 */
HYSCAN_API
HyScanSourceType        hyscan_nmea_data_get_source             (HyScanNMEAData        *data);

/**
 *
 * Функция возвращает номер канала для используемого источника данных.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink.
 *
 * \return Номер канала.
 *
 */
HYSCAN_API
guint                   hyscan_nmea_data_get_channel            (HyScanNMEAData        *data);

/**
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink.
 *
 * \return TRUE - если возможна запись данных, FALSE - если данные в режиме только чтения.
 *
 */
HYSCAN_API
gboolean                hyscan_nmea_data_is_writable            (HyScanNMEAData        *data);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink;
 * \param first указатель на переменную для начального индекса или NULL;
 * \param last указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                hyscan_nmea_data_get_range              (HyScanNMEAData        *data,
                                                                 guint32               *first,
                                                                 guint32               *last);

/**
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return TRUE - если данные найдены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
HyScanDBFindStatus      hyscan_nmea_data_find_data              (HyScanNMEAData        *data,
                                                                 gint64                 time,
                                                                 guint32               *lindex,
                                                                 guint32               *rindex,
                                                                 gint64                *ltime,
                                                                 gint64                *rtime);

/**
 *
 * Функция возвращает указатель на NMEA-строку.
 *
 * Строка будет гарантированно нуль-терминирована.
 * Владельцем строки является класс, запрещено освобождать эту память.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink;
 * \param index индекс считываемых данных;
 * \param size указатель на переменную для сохранения размера считанных данных или NULL;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return указатель на константную нуль-терминированную строку.
 *
 */
HYSCAN_API
const gchar            *hyscan_nmea_data_get_sentence           (HyScanNMEAData        *data,
                                                                 guint32                index,
                                                                 guint32               *size,
                                                                 gint64                *time);

/**
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться на значение
 * номера изменения, важен только факт смены номера по сравнению с предыдущим запросом.
 *
 * \param data указатель на объект \link HyScanNMEAData \endlink.
 *
 * \return Номер изменения.
 *
 */
HYSCAN_API
guint32                 hyscan_nmea_data_get_mod_count          (HyScanNMEAData        *data);

/**
 *
 * Функция верифицирует NMEA-строку.
 *
 * Внутри функция проверяет контрольную сумму и тип строки.
 * Если тип сообщения совпадает с DPT, RMC или GGA, будут возвращены соответственно
 * HYSCAN_SOURCE_NMEA_DPT, HYSCAN_SOURCE_NMEA_GGA, HYSCAN_SOURCE_NMEA_RMC.
 * Если тип сообщения не соотвествует перечисленным выше, но при этом контрольная
 * сумма совпала, будет возвращено HYSCAN_SOURCE_NMEA_ANY.
 * Если контрольная сумма не совпала, будет возвращено HYSCAN_SOURCE_INVALID.
 *
 * \param sentence указатель на строку.
 *
 * \return тип строки.
 *
 */
HYSCAN_API
HyScanSourceType        hyscan_nmea_data_check_sentence         (const gchar           *sentence);

/**
 *
 * Функция разбивает строку, содержащую несколько NMEA-сообщений, на отдельные строки.
 *
 * В канале HYSCAN_SOURCE_NMEA_ANY по одному индексу может находиться сразу несколько
 * сообщений. Эта функция генерирует нуль-терминированный вектор строк.
 *
 * Владельцем строки становится пользователь и после использования её нужно освободить
 * функцией g_strfreev.
 *
 * \param sentence указатель на строку;
 * \param length длина строки.
 *
 * \return указатель на нуль-терминированный список NMEA-строк.
 *
 */
HYSCAN_API
gchar                 **hyscan_nmea_data_split_sentence        (const gchar           *sentence,
                                                                guint32                length);

G_END_DECLS

#endif /* __HYSCAN_NMEA_DATA_H__ */

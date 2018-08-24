/**
 * \file hyscan-ndata.h
 *
 * \brief Заголовочный файл интерфейса HyScanNavData
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanNavData HyScanNavData - интерфейс для получения навигационных данных.
 *
 * Интерфейс \link HyScanNavData \endlink предназначен для всех элементарных классов,
 * реализующих доступ к навигационным данным: широте, долготе, курсу, глубине и так далее.
 * Под элементарными в данном случае подразумеваются все те классы, которые не производят
 * сглаживание и усреднение данных, а только обрабатывают строки данных и выдают
 * одно и только одно значение. Иными словами, эти классы оперируют индексами,
 * в то время как более высокоуровневые классы оперируют временем. Все классы,
 * реализующие этот интерфейс являются надстройками над БД, возвращающими не целые строки
 * данных, а обработанные.
 *
 * Для того, чтобы более высокоуровневые классы могли отличать данные, полученные
 * из разных источников (но одним интерфейсом), вводится понятие токена. Токен - это
 * строка, однозначно определяющая внутреннее состояние класса. У двух объектов,
 * обрабатывающих один и тот же навигационный параметр с одинаковыми параметрами
 * токен должен быть одинаковым. Например, курс можно извлекать из RMC-строк или
 * вычислять из координат. В этом случае токен должен различаться.
 *
 * Публично доступны следующие методы:
 * - #hyscan_nav_data_set_cache - установка системы кэширования;
 * - #hyscan_nav_data_get - получение глубины;
 * - #hyscan_nav_data_find_data - поиск данных;
 * - #hyscan_nav_data_get_range - определение диапазона;
 * - #hyscan_nav_data_get_position - получение местоположения приемной антенны;
 * - #hyscan_nav_data_is_writable - определение возможности дозаписи в канал данных;
 * - #hyscan_nav_data_get_token - получение токена;
 * - #hyscan_nav_data_get_mod_count - получение значения счётчика изменений.
 */

#ifndef __HYSCAN_NAV_DATA_H__
#define __HYSCAN_NAV_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAV_DATA            (hyscan_nav_data_get_type ())
#define HYSCAN_NAV_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_DATA, HyScanNavData))
#define HYSCAN_IS_NAV_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_DATA))
#define HYSCAN_NAV_DATA_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_NAV_DATA, HyScanNavDataInterface))

typedef struct _HyScanNavData HyScanNavData;
typedef struct _HyScanNavDataInterface HyScanNavDataInterface;

struct _HyScanNavDataInterface
{
  GTypeInterface            g_iface;

  void                    (*set_cache)                    (HyScanNavData       *ndata,
                                                           HyScanCache         *cache);
  gboolean                (*get)                          (HyScanNavData       *ndata,
                                                           guint32              index,
                                                           gint64              *time,
                                                           gdouble             *value);
  HyScanDBFindStatus      (*find_data)                    (HyScanNavData       *ndata,
                                                           gint64               time,
                                                           guint32             *lindex,
                                                           guint32             *rindex,
                                                           gint64              *ltime,
                                                           gint64              *rtime);
  gboolean                (*get_range)                    (HyScanNavData       *ndata,
                                                           guint32             *first,
                                                           guint32             *last);
  HyScanAntennaPosition   (*get_position)                 (HyScanNavData       *ndata);
  gboolean                (*is_writable)                  (HyScanNavData       *ndata);
  const gchar            *(*get_token)                    (HyScanNavData       *ndata);
  guint32                 (*get_mod_count)                (HyScanNavData       *ndata);
};

HYSCAN_API
GType                   hyscan_nav_data_get_type        (void);

/**
 *
 * Функция задаёт используемый кэш и префикс идентификаторов объектов для
 * хранения в нём.
 *
 * \param ndata указатель на объект \link HyScanNavData \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink или NULL;
 *
 */
HYSCAN_API
void                    hyscan_nav_data_set_cache       (HyScanNavData          *ndata,
                                                         HyScanCache            *cache);
/**
 *
 * Функция возвращает значение для заданного индекса.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink;
 * \param index индекс записи в канале данных;
 * \param time время данных;
 * \param value значение.
 *
 * \return TRUE, если удалось определить, FALSE в случае ошибки.
 *
 */
HYSCAN_API
gboolean                hyscan_nav_data_get             (HyScanNavData         *ndata,
                                                         guint32                index,
                                                         gint64                *time,
                                                         gdouble               *value);

/**
 *
 * Функция ищет индекс данных для указанного момента времени. Функция возвращает статус поиска
 * индекса данных \link HyScanDBFindStatus \endlink.
 *
 * Если найдены данные, метка времени которых точно совпадает с указанной, значения lindex и rindex,
 * а также ltime и rtime будут одинаковые.
 *
 * Если найдены данные, метка времени которых находится в переделах записанных данных, значения
 * lindex и ltime будут указывать на индекс и время соответственно, данных, записанных перед искомым
 * моментом времени. А rindex и ltime будут указывать на индекс и время соответственно, данных, записанных
 * после искомого момента времени.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return Статус поиска индекса данных.
 *
 */
HYSCAN_API
HyScanDBFindStatus      hyscan_nav_data_find_data       (HyScanNavData         *ndata,
                                                         gint64                 time,
                                                         guint32               *lindex,
                                                         guint32               *rindex,
                                                         gint64                *ltime,
                                                         gint64                *rtime);

/**
 *
 * Функция возвращает диапазон данных в используемом канале данных.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink;
 * \param first указатель на переменную для сохранения самого раннего индекса данных или NULL;
 * \param last указатель на переменную для сохранения самого позднего индекса данных или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                hyscan_nav_data_get_range       (HyScanNavData         *ndata,
                                                         guint32               *first,
                                                         guint32               *last);

/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны
 * в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink.
 *
 * \return Местоположение приёмной антенны.
 *
 */
HYSCAN_API
HyScanAntennaPosition   hyscan_nav_data_get_position    (HyScanNavData         *ndata);

/**
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink.
 *
 * \return TRUE - если возможна запись данных, FALSE - если данные в режиме только чтения.
 *
 */
HYSCAN_API
gboolean                hyscan_nav_data_is_writable     (HyScanNavData         *ndata);

/**
 *
 * Функция возвращает строку, содержащую идентификатор внутренних параметров класса,
 * реализующего интерфейс.
 *
 * Одни данные глубины могут быть получены из разных источников: NMEA, ГБО, эхолот и т.д.
 * Возможно сделать реализацию, определяющую глубину сразу из нескольких источников
 * (например, левый и правый борт одновременно). Чтобы объекты более высокого уровня ничего
 * не знали о реально используемых источниках, используется этот идентификатор.
 *
 * Строка должна содержать путь к БД, проект, галс и все внутренние параметры объекта.
 * Требуется, чтобы длина этой строки была постоянна (в пределах одного и того же) галса.
 * Поэтому значения внутренних параметров объекта нужно пропускать через некую хэш-функцию.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink.
 *
 * \return указатель на строку с идентификатором.
 *
 */
HYSCAN_API
const gchar            *hyscan_nav_data_get_token       (HyScanNavData         *ndata);

/**
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться на значение
 * номера изменения, важен только факт смены номера по сравнению с предыдущим запросом.
 *
 * Изменениями считаются события в базе данных, связанные с этим каналом.
 *
 * \param ndata указатель на интерфейс \link HyScanNavData \endlink;
 *
 * \return значение счётчика изменений.
 *
 */
HYSCAN_API
guint32                 hyscan_nav_data_get_mod_count   (HyScanNavData          *ndata);

G_END_DECLS

#endif /* __HYSCAN_NAV_DATA_H__ */

/**
 * \file hyscan-depth.h
 *
 * \brief Заголовочный файл интерфейса HyScanDepth
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDepth HyScanDepth - интерфейс для получения данных о глубине.
 *
 * Интерфейс \link HyScanDepth \endlink предназначен для всех элементарных классов,
 * реализующих определение глубины. Под элементарными в данном случае подразумеваются все
 * те классы, которые не производят сглаживание и усреднение данных, а только обрабатывают
 * строки данных и выдают одно значение. Иными словами, эти классы оперируют индексами,
 * в то время как более высокоуровневые классы оперируют временем. По сути, все классы,
 * реализующие этот интерфейс являются надстройками над БД, возвращающими не целые строки
 * данных, а обработанные.
 *
 * Для того, чтобы более высокоуровневые классы могли отличать данные, полученные
 * из разных источников (но одним интерфейсом), вводится понятие токена. Токен - это
 * строка, однозначно определяющая внутреннее состояние класса. У двух объектов,
 * обрабатывающих один и тот же галс с одинаковыми параметрами токен должен быть одинаковым.
 *
 * Публично доступны следующие методы:
 * - #hyscan_depth_set_sound_velocity - установка профиля скорости звука;
 * - #hyscan_depth_set_cache - установка системы кэширования;
 * - #hyscan_depth_get - получение глубины;
 * - #hyscan_depth_find_data - поиск данных;
 * - #hyscan_depth_get_range - определение диапазона;
 * - #hyscan_depth_get_position - получение местоположения приемной антенны;
 * - #hyscan_depth_is_writable - определение возможности дозаписи в канал данных;
 * - #hyscan_depth_get_token - получение токена;
 * - #hyscan_depth_get_mod_count - получение значения счётчика изменений.
 */

#ifndef __HYSCAN_DEPTH_H__
#define __HYSCAN_DEPTH_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEPTH            (hyscan_depth_get_type ())
#define HYSCAN_DEPTH(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEPTH, HyScanDepth))
#define HYSCAN_IS_DEPTH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEPTH))
#define HYSCAN_DEPTH_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_DEPTH, HyScanDepthInterface))

typedef struct _HyScanDepth HyScanDepth;
typedef struct _HyScanDepthInterface HyScanDepthInterface;

struct _HyScanDepthInterface
{
  GTypeInterface            g_iface;

  void                    (*set_sound_velocity)           (HyScanDepth         *depth,
                                                           HyScanSoundVelocity *velocity,
                                                           guint                length);
  void                    (*set_cache)                    (HyScanDepth         *depth,
                                                           HyScanCache         *cache,
                                                           const gchar         *prefix);
  gdouble                 (*get)                          (HyScanDepth         *depth,
                                                           guint32              index,
                                                           gint64              *time);
  HyScanDBFindStatus      (*find_data)                    (HyScanDepth         *depth,
                                                           gint64               time,
                                                           guint32             *lindex,
                                                           guint32             *rindex,
                                                           gint64              *ltime,
                                                           gint64              *rtime);
  gboolean                (*get_range)                    (HyScanDepth         *depth,
                                                           guint32             *first,
                                                           guint32             *last);
  HyScanAntennaPosition   (*get_position)                 (HyScanDepth         *depth);
  gboolean                (*is_writable)                  (HyScanDepth         *depth);
  const gchar            *(*get_token)                    (HyScanDepth         *depth);
  guint32                 (*get_mod_count)                (HyScanDepth         *depth);
};

HYSCAN_API
GType                   hyscan_depth_get_type           (void);

/**
 *
 * Функция задаёт профиль скорости звука. При этом не гарантируется,
 * что объект поддерживает установку скорости звука.
 *
 * \param depth указатель на объект \link HyScanDepth \endlink;
 * \param velocity массив структур \link HyScanSoundVelocity \endlink;
 * \param length число элементов.
 *
 */
HYSCAN_API
void                    hyscan_depth_set_sound_velocity (HyScanDepth            *depth,
                                                         HyScanSoundVelocity    *velocity,
                                                         guint                   length);


/**
 *
 * Функция задаёт используемый кэш и префикс идентификаторов объектов для
 * хранения в нём.
 *
 * \param depth указатель на объект \link HyScanDepth \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink или NULL;
 * \param prefix префикс ключа кэширования или NULL.
 *
 */
HYSCAN_API
void                    hyscan_depth_set_cache          (HyScanDepth            *depth,
                                                         HyScanCache            *cache,
                                                         const gchar            *prefix);
/**
 *
 * Функция возвращает значение глубины для заданного индекса.
 *
 * В случае какой-либо ошибки функция возвращает -1.0.

 * \param depth указатель на интерфейс \link HyScanDepth \endlink;
 * \param index индекс записи в канале данных;
 * \param time время данных.
 *
 * \return Значение глубины в метрах.
 *
 */
HYSCAN_API
gdouble                 hyscan_depth_get                (HyScanDepth           *depth,
                                                         guint32                index,
                                                         gint64                *time);

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
 * \param depth указатель на интерфейс \link HyScanDepth \endlink;
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
HyScanDBFindStatus      hyscan_depth_find_data          (HyScanDepth           *depth,
                                                         gint64                 time,
                                                         guint32               *lindex,
                                                         guint32               *rindex,
                                                         gint64                *ltime,
                                                         gint64                *rtime);

/**
 *
 * Функция возвращает диапазон данных в используемом канале данных.
 *
 * \param depth указатель на интерфейс \link HyScanDepth \endlink;
 * \param first указатель на переменную для сохранения самого раннего индекса данных или NULL;
 * \param last указатель на переменную для сохранения самого позднего индекса данных или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                hyscan_depth_get_range          (HyScanDepth           *depth,
                                                         guint32               *first,
                                                         guint32               *last);

/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны
 * в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param depth указатель на интерфейс \link HyScanDepth \endlink.
 *
 * \return Местоположение приёмной антенны.
 *
 */
HYSCAN_API
HyScanAntennaPosition   hyscan_depth_get_position       (HyScanDepth           *depth);

/**
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * \param depth указатель на интерфейс \link HyScanDepth \endlink.
 *
 * \return TRUE - если возможна запись данных, FALSE - если данные в режиме только чтения.
 *
 */
HYSCAN_API
gboolean                hyscan_depth_is_writable        (HyScanDepth           *depth);

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
 * \param depth указатель на интерфейс \link HyScanDepth \endlink.
 *
 * \return указатель на строку с идентификатором.
 *
 */
HYSCAN_API
const gchar            *hyscan_depth_get_token          (HyScanDepth           *depth);

/**
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться на значение
 * номера изменения, важен только факт смены номера по сравнению с предыдущим запросом.
 *
 * Изменениями считаются события в базе данных, связанные с этим каналом.
 *
 * \param depth указатель на интерфейс \link HyScanDepth \endlink;
 *
 * \return значение счётчика изменений.
 *
 */
HYSCAN_API
guint32                 hyscan_depth_get_mod_count      (HyScanDepth            *depth);

G_END_DECLS

#endif /* __HYSCAN_DEPTH_H__ */

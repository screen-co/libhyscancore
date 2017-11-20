/**
 * \file hyscan-forward-look-data.h
 *
 * \brief Заголовочный файл класса обработки данных вперёдсмотрящего локатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanForwardLookData HyScanForwardLookData - класс обработки данных вперёдсмотрящего локатора
 *
 * Класс HyScanForwardLookData используется для получения обработанных данных вперёдсмотрящего
 * локатора, таких как угол и дистанция до цели, а также интенсивность отражения. Данные
 * возвращаются в виде массива структур \link HyScanForwardLookDOA \endlink. Создание объекта
 * производится с помощью функции #hyscan_forward_look_data_new.
 *
 * В своей работе класс HyScanForwardLookData может использовать внешний кэш для хранения обработанных
 * данных. В случае запроса ранее обработанных данных пользователь получит их копию из кэша. Несколько
 * экземпляров класса HyScanForwardLookData, обрабатывающих одинаковые каналы данных могут использовать
 * один и тот же кэш. Таким образом, данные обработанные в одной части приложения не потребуют повторной
 * обработки в другой его части. Для задания кэша предназанчена функция #hyscan_forward_look_data_set_cache.
 *
 * Местоположение приёмной антенны гидролокатора можно получить функцией #hyscan_forward_look_data_get_position.
 *
 * Класс поддерживает обработку данных, запись которых производится в настоящий момент. В этом случае
 * могут появляться новые данные или исчезать уже записанные. Определить возможность изменения данных
 * можно с помощью функции #hyscan_forward_look_data_is_writable.
 *
 * Номер изменения в данных можно получить с помоью функции #hyscan_forward_look_data_get_mod_count.
 *
 * Для точной обработки данных необходимо установить скорость звука в воде, для этих целей используется
 * функция #hyscan_forward_look_data_set_sound_velocity. По умолчанию используется значение 1500 м/с.
 *
 * Данные от вперёдсмотрящего локатора находятся в секторе обзора, величину которого можно узнать с
 * помощью функции #hyscan_forward_look_data_get_alpha.
 *
 * Функции #hyscan_forward_look_data_get_range и #hyscan_forward_look_data_find_data используются для
 * определения границ записанных данных и их поиска по метке времени. Эти функции аналогичны функциям
 * \link hyscan_db_channel_get_data_range \endlink и \link hyscan_db_channel_find_data \endlink интерфейса
 * \link HyScanDB \endlink.
 *
 * Для получения данных используется функция #hyscan_forward_look_data_get_doa_values.
 *
 * HyScanForwardLookData не поддерживает работу в многопоточном режиме. Рекомендуется создавать свой
 * экземпляр объекта обработки данных в каждом потоке и использовать единый кэш данных.
 *
 */

#ifndef __HYSCAN_FORWARD_LOOK_DATA_H__
#define __HYSCAN_FORWARD_LOOK_DATA_H__

#include <hyscan-db.h>
#include <hyscan-types.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_FORWARD_LOOK_DATA             (hyscan_forward_look_data_get_type ())
#define HYSCAN_FORWARD_LOOK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_FORWARD_LOOK_DATA, HyScanForwardLookData))
#define HYSCAN_IS_FORWARD_LOOK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_FORWARD_LOOK_DATA))
#define HYSCAN_FORWARD_LOOK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_FORWARD_LOOK_DATA, HyScanForwardLookDataClass))
#define HYSCAN_IS_FORWARD_LOOK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_FORWARD_LOOK_DATA))
#define HYSCAN_FORWARD_LOOK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_FORWARD_LOOK_DATA, HyScanForwardLookDataClass))

typedef struct _HyScanForwardLookData HyScanForwardLookData;
typedef struct _HyScanForwardLookDataPrivate HyScanForwardLookDataPrivate;
typedef struct _HyScanForwardLookDataClass HyScanForwardLookDataClass;

/** \brief Точка цели вперёдсмотрящего локатора (Direction Of Arrival) */
typedef struct
{
  gfloat angle;                                /**< Азимут цели относительно перпендикуляра к антенне, рад. */
  gfloat distance;                             /**< Дистанция до цели, метры. */
  gfloat amplitude;                            /**< Амплитуда отражённого сигнала. */
} HyScanForwardLookDOA;

struct _HyScanForwardLookData
{
  GObject parent_instance;

  HyScanForwardLookDataPrivate *priv;
};

struct _HyScanForwardLookDataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_forward_look_data_get_type               (void);

/**
 *
 * Функция создаёт новый объект обработки данных вперёдсмотрящего локатора. Если данные
 * отсутствуют, функция вернёт значение NULL.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param raw использовать - TRUE или нет - FALSE сырые данные.
 *
 * \return Указатель на объект \link HyScanForwardLookData \endlink или NULL.
 *
 */
HYSCAN_API
HyScanForwardLookData         *hyscan_forward_look_data_new                    (HyScanDB              *db,
                                                                                const gchar           *project_name,
                                                                                const gchar           *track_name,
                                                                                gboolean               raw);

/**
 *
 * Функция задаёт используемый кэш и префикс идентификаторов объектов для
 * хранения в нём.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink или NULL;
 * \param prefix префикс ключа кэширования или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_data_set_cache              (HyScanForwardLookData *data,
                                                                                HyScanCache           *cache,
                                                                                const gchar           *prefix);

/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны гидролокатора
 * в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink.
 *
 * \return Местоположение приёмной антенны гидролокатора.
 *
 */
HYSCAN_API
HyScanAntennaPosition          hyscan_forward_look_data_get_position           (HyScanForwardLookData *data);

/**
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink.
 *
 * \return TRUE - если возможна запись данных, FALSE - если данные в режиме только чтения.
 *
 */
HYSCAN_API
gboolean                       hyscan_forward_look_data_is_writable            (HyScanForwardLookData *data);

/**
 *
 * Функция задаёт скорость звука в воде, используемую для обработки данных.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink;
 * \param sound_velocity скорость звука, м/с.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_data_set_sound_velocity     (HyScanForwardLookData *data,
                                                                                gdouble                sound_velocity);

/**
 *
 * Функция возвращает угол, определяющий сектор обзора локатора в горизонтальной плоскости.
 * Сектор обзора составляет [-angle +angle], где angle - значение возвращаемой этой функцией.
 * Сектор обзора зависит от скорости звука.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink.
 *
 * \return Угол сектора обзора, рад.
 *
 */
HYSCAN_API
gdouble                        hyscan_forward_look_data_get_alpha              (HyScanForwardLookData *data);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink;
 * \param first_index указатель на переменную для начального индекса или NULL;
 * \param last_index указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_forward_look_data_get_range              (HyScanForwardLookData *data,
                                                                                guint32               *first_index,
                                                                                guint32               *last_index);

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
guint32                        hyscan_forward_look_data_get_mod_count          (HyScanForwardLookData *data);

/**
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink;
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
HyScanDBFindStatus             hyscan_forward_look_data_find_data              (HyScanForwardLookData *data,
                                                                                gint64                 time,
                                                                                guint32               *lindex,
                                                                                guint32               *rindex,
                                                                                gint64                *ltime,
                                                                                gint64                *rtime);

/**
 *
 * Функция возвращает данные вперёдсмотрящего локатора.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором действительны до
 * следующего вызова этой функции. Пользователь не должен модифицировать эти данные.
 *
 * \param data указатель на объект \link HyScanForwardLookData \endlink;
 * \param index индекс считываемых данных;
 * \param n_points число точек данных;
 * \param time метка времени считанных данных или NULL.
 *
 * \return Данные вперёдсмотрящего локатора или NULL.
 *
 */
HYSCAN_API
const HyScanForwardLookDOA    *hyscan_forward_look_data_get_doa_values         (HyScanForwardLookData *data,
                                                                                guint32                index,
                                                                                guint32               *n_points,
                                                                                gint64                *time);

G_END_DECLS

#endif /* __HYSCAN_FORWARD_LOOK_DATA_H__ */

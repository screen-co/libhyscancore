/**
 * \file hyscan-state.h
 *
 * \brief Заголовочный файл класса состояния проекта HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanState HyScanState - параметры состояния комплекса программ HyScan
 *
 * Класс HyScanState может использоваться для хранения значений параметров состояния комплекса
 * программ HyScan. В рамках одной программы создаётся один экземпляр HyScanState, который
 * используется всеми остальными объектами. При изменении каких-либо параметров состояния
 * посылаются соответствующие сигналы.
 *
 * Класс HyScanState может использоваться только одним потоком. Рекомендуется использовать
 * его в основном цикле Glib или Gtk.
 *
 * HyScanState хранит следующие GObject параметры:
 *
 * - "db" - указатель на интерфейс базы данных \link HyScanDB \endlink, сигнал при изменении - "db-changed";
 * - "cache" - указатель на интерфейс системы кэширования \link HyScanCache \endlink, сигнал при изменении - "cache-changed";
 * - "project_name" - название текущего открытого проекта, сигнал при изменении - "project-changed";
 * - "track_name" - название текущего обрабатываемого галса, сигнал при изменении - "track-changed";
 * - "preset_name" - название группы параметров используемой для текущей обработки данных проекта (preset), сигнал при изменении - "preset-changed";
 * - "profile_name" - название текущего профиля задачи, сигнал при изменении - "profile-changed".
 *
 * В качестве параметра сигнала "db-changed" передаётся указатель на интерфейс \link HyScanDB \endlink,
 * сигнала "cache-changed" передаётся указатель на интерфейс \link HyScanCache \endlink.
 * Для остальных сигналов передаётся указатель на строку с текущим значением параметра.
 *
 * Прототипы callback функции для сигналов:
 *
 * - "db-changed" - void callback_function( HyScanState *state, HyScanDb *db, gpointer user_data );
 * - "cache-changed" - void callback_function( HyScanState *state, HyScanCache *db, gpointer user_data );
 * - остальные - void callback_function( HyScanState *state, const gchar *name, gpointer user_data );
 *
 * Сигналы посылаются только если новое значение параметра отличается от предыдущего.
 *
 * HyScanState хранит только основные параметры используемые программами. Данные, параметры их обработки и
 * пользовательские настройки программные модули должны считывать самостоятельно из базы данных или
 * файлов профилей. Все настройки обработки данных проекта должны храниться в виде параметров проекта
 * в базе данных. В проекте может храниться несколько вариантов обработки данных (preset). Текущий
 * вариант определяется именем группы параметров обработки. Возможности программных модулей и настройки
 * обработки по умолчанию должны храниться в конфигурационных файлах однозначно идентифицируемых
 * по имени текущего профиля задачи.
 *
 * Все программные модули должны динамически изменять своё поведение при смене значений параметров HyScanState,
 * параметров обработки из базы данных и изменении профиля задачи.
 *
 * Параметры могут считываться и изменяться функциями g_object_get и g_object_set или отдельными функциями класса.
 * Класс HyScanState содержит следующие функции для изменения параметров и получения их текущих значений:
 *
 * - #hyscan_state_set_db - изменить указатель на интерфейс базы данных;
 * - #hyscan_state_get_db - получить указатель на интерфейс базы данных;
 * - #hyscan_state_set_cache - изменить указатель на интерфейс системы кэширования;
 * - #hyscan_state_get_cache - получить указатель на интерфейс системы кэширования;
 * - #hyscan_state_set_project_name - изменить название текущего проекта;
 * - #hyscan_state_get_project_name - получить название текущего проекта;
 * - #hyscan_state_set_track_name - изменить название текущего галса;
 * - #hyscan_state_get_track_name - получить название текущего галса;
 * - #hyscan_state_set_preset_name - изменить название группы параметров обработки;
 * - #hyscan_state_get_preset_name - получить название группы параметров обработки;
 * - #hyscan_state_set_profile_name - изменить название профиля задачи;
 * - #hyscan_state_get_profile_name - получить название профиля задачи.
 *
 * Создать новый объект HyScanState можно функцией #hyscan_state_new.
 *
 */

#ifndef __HYSCAN_STATE_H__
#define __HYSCAN_STATE_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_STATE             (hyscan_state_get_type ())
#define HYSCAN_STATE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_STATE, HyScanState))
#define HYSCAN_IS_STATE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_STATE))
#define HYSCAN_STATE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_STATE, HyScanStateClass))
#define HYSCAN_IS_STATE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_STATE))
#define HYSCAN_STATE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_STATE, HyScanStateClass))

typedef struct _HyScanState HyScanState;
typedef struct _HyScanStatePrivate HyScanStatePrivate;
typedef struct _HyScanStateClass HyScanStateClass;

struct _HyScanState
{
  GObject parent_instance;

  HyScanStatePrivate *priv;
};

struct _HyScanStateClass
{
  GObjectClass parent_class;
};

HYSCAN_CORE_EXPORT
GType hyscan_state_get_type (void);

/**
 *
 * Функция создаёт новый объект \link HyScanState \endlink.
 *
 * Объект необходимо удалить функцией g_object_unref.
 *
 * \return Указатель на объект \link HyScanState \endlink.
 *
 */
HYSCAN_CORE_EXPORT
HyScanState   *hyscan_state_new                (void);

/**
 *
 * Функция задаёт новый указатель на интерфейс базы данных \link HyScanDB \endlink.
 *
 * При изменении указателя на интерфейс базы данных производится проверка типа объекта и если он совпадает
 * с HYSCAN_TYPE_DB указатель изменяется на заданный. В противном случае значение указателя изменяется на NULL.
 * Также, при изменении указателя на интерфейс базы данных, обнуляются имена проекта, группы параметров обработки
 * и галса.
 *
 * При задании нового указателя число ссылок на старый объект уменьшается функцией g_object_unref, а на новый
 * увеличивается функцией g_object_ref. Увеличивать и уменьшать число ссылок на базу данных, также, должны все
 * пользователи базы данных.
 *
 * Допускается передать NULL в качестве ссылки на интерфейс базы данных.
 *
 * \param state указатель на объект \link HyScanState \endlink;
 * \param db указатель на объект \link HyScanDB \endlink или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_CORE_EXPORT
void           hyscan_state_set_db             (HyScanState           *state,
                                                HyScanDB              *db);

/**
 *
 * Функция возвращает указатель на интерфейс базы данных \link HyScanDB \endlink.
 *
 * Перед началом использования необходимо увеличить счётчик ссылок на объект \link HyScanDB \endlink функцией g_object_ref, а
 * по окончании уменьшить функцией g_object_unref.
 *
 * \param state указатель на объект \link HyScanState \endlink.
 *
 * \return Указатель на объект \link HyScanDB \endlink или NULL.
 *
 */
HYSCAN_CORE_EXPORT
HyScanDB      *hyscan_state_get_db             (HyScanState           *state);

/**
 *
 * Функция задаёт новый указатель на интерфейс системы кэширования \link HyScanCache \endlink.
 *
 * При изменении указателя на интерфейс системы кэширования производится проверка типа объекта и если он совпадает
 * с HYSCAN_TYPE_CACHE указатель изменяется на заданный. В противном случае значение указателя изменяется на NULL.
 *
 * При задании нового указателя число ссылок на старый объект уменьшается функцией g_object_unref, а на новый
 * увеличивается функцией g_object_ref. Увеличивать и уменьшать число ссылок на базу данных, также, должны все
 * пользователи системы кэширования.
 *
 * Допускается передать NULL в качестве ссылки на интерфейс системы кэширования.
 *
 * \param state указатель на объект \link HyScanState \endlink;
 * \param cache указатель на объект \link HyScanCache \endlink или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_CORE_EXPORT
void           hyscan_state_set_cache          (HyScanState           *state,
                                                HyScanCache           *cache);

/**
 *
 * Функция возвращает указатель на интерфейс системы кэширования \link HyScanCache \endlink.
 *
 * Перед началом использования необходимо увеличить счётчик ссылок на объект \link HyScanCache \endlink функцией g_object_ref, а
 * по окончании уменьшить функцией g_object_unref.
 *
 * \param state указатель на объект \link HyScanState \endlink.
 *
 * \return Указатель на объект \link HyScanCache \endlink или NULL.
 *
 */
HYSCAN_CORE_EXPORT
HyScanCache   *hyscan_state_get_cache          (HyScanState           *state);

/**
 *
 * Функция задаёт имя используемого проекта.
 *
 * При изменении имени проекта обнуляются имена галса и группы параметров обработки.
 *
 * \param state указатель на объект \link HyScanState \endlink;
 * \param project_name имя используемого проекта или NULL.
 *
 * \return Нет.
 *
*/
HYSCAN_CORE_EXPORT
void           hyscan_state_set_project_name   (HyScanState           *state,
                                                const gchar           *project_name);

/**
 *
 * Функция возвращает имя используемого проекта.
 *
 * Возвращаемая строка принадлежит объекту \link HyScanState \endlink, пользователь должен
 * самостоятельно сделать копию строки при необходимости.
 *
 * \param state указатель на объект \link HyScanDB \endlink.
 *
 * \return Имя используемого проекта или NULL.
 *
 */
HYSCAN_CORE_EXPORT
const gchar   *hyscan_state_get_project_name   (HyScanState           *state);

/**
 *
 * Функция задаёт имя используемого галса.
 *
 * \param state указатель на объект \link HyScanState \endlink;
 * \param track_name имя используемого галса или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_CORE_EXPORT
void           hyscan_state_set_track_name     (HyScanState           *state,
                                                const gchar           *track_name);

/**
 *
 * Функция возвращает имя используемого галса.
 *
 * Возвращаемая строка принадлежит объекту \link HyScanState \endlink, пользователь должен
 * самостоятельно сделать копию строки при необходимости.
 *
 * \param state указатель на объект \link HyScanDB \endlink.
 *
 * \return Имя используемого галса или NULL.
 *
 */
HYSCAN_CORE_EXPORT
const gchar   *hyscan_state_get_track_name     (HyScanState           *state);

/**
 *
 * Функция задаёт имя группы параметров обработки.
 *
 * \param state указатель на объект \link HyScanState \endlink;
 * \param preset_name имя группы параметров обработки или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_CORE_EXPORT
void           hyscan_state_set_preset_name    (HyScanState           *state,
                                                const gchar           *preset_name);

/**
 *
 * Функция возвращает имя группы параметров обработки.
 *
 * Возвращаемая строка принадлежит объекту \link HyScanState \endlink, пользователь должен
 * самостоятельно сделать копию строки при необходимости.
 *
 * \param state указатель на объект \link HyScanDB \endlink.
 *
 * \return Имя группы параметров обработки или NULL.
 *
 */
HYSCAN_CORE_EXPORT
const gchar   *hyscan_state_get_preset_name    (HyScanState           *state);

/**
 *
 * Функция задаёт имя профиля задачи.
 *
 * \param state указатель на объект \link HyScanState \endlink;
 * \param profile_name имя профиля задачи или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_CORE_EXPORT
void           hyscan_state_set_profile_name   (HyScanState           *state,
                                                const gchar           *profile_name);

/**
 *
 * Функция возвращает имя профиля задачи.
 *
 * Возвращаемая строка принадлежит объекту \link HyScanState \endlink, пользователь должен
 * самостоятельно сделать копию строки при необходимости.
 *
 * \param state указатель на объект \link HyScanDB \endlink.
 *
 * \return Имя профиля задачи или NULL.
 *
 */
HYSCAN_CORE_EXPORT
const gchar   *hyscan_state_get_profile_name   (HyScanState           *state);

G_END_DECLS

#endif /* __HYSCAN_STATE_H__ */

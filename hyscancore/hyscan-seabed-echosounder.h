/**
 * \file hyscan-seabed-echosounder.h
 *
 * \brief Заголовочный файл класса обработки акустических данных
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSeabedEchosounder HyScanSeabedEchosounder - класс определения глубины по данным эхолота.
 *
 * В сочетании с интерфейсом класс позволяет всего лишь с помощью двух комманд узнавать глубину для заданного
 * индекса и не задумываться обо всём остальном. Публично доступна только одна команда:
 * #hyscan_seabed_echosounder_new, создающая объект получения глубины.
 *  Дальнейшее общение с объектом ведется через интерфейс \link HyScanSeabed \endlink.
 */

#ifndef __HYSCAN_SEABED_ECHOSOUNDER_H__
#define __HYSCAN_SEABED_ECHOSOUNDER_H__

#include <hyscan-seabed.h>
#include <hyscan-core-exports.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>


G_BEGIN_DECLS

#define HYSCAN_TYPE_SEABED_ECHOSOUNDER             (hyscan_seabed_echosounder_get_type ())
#define HYSCAN_SEABED_ECHOSOUNDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SEABED_ECHOSOUNDER, HyScanSeabedEchosounder))
#define HYSCAN_IS_SEABED_ECHOSOUNDER(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SEABED_ECHOSOUNDER))
#define HYSCAN_SEABED_ECHOSOUNDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SEABED_ECHOSOUNDER, HyScanSeabedEchosounderClass))
#define HYSCAN_IS_SEABED_ECHOSOUNDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SEABED_ECHOSOUNDER))
#define HYSCAN_SEABED_ECHOSOUNDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SEABED_ECHOSOUNDER, HyScanSeabedEchosounderClass))

typedef struct _HyScanSeabedEchosounder HyScanSeabedEchosounder;
typedef struct _HyScanSeabedEchosounderPrivate HyScanSeabedEchosounderPrivate;
typedef struct _HyScanSeabedEchosounderClass HyScanSeabedEchosounderClass;

struct _HyScanSeabedEchosounder
{
  GObject parent_instance;

  HyScanSeabedEchosounderPrivate *priv;
};

struct _HyScanSeabedEchosounderClass
{
  GObjectClass parent_class;
};

HYSCAN_CORE_EXPORT
GType           hyscan_seabed_echosounder_get_type      (void);

/**
 *
 * Функция создаёт новый объект определения глубины по эхолоту.
 *
 * Если при создании объекта передан указатель на интерфейс системы кэширования, она будет
 * использоваться. Данные кэшируются по ключу вида "cache_prefix.project.track.channel.type.index", где *
 * - cache_prefix - опциональный префикс ключа или NULL;
 * - project - имя проекта;
 * - track - имя галса;
 * - channel - имя канала данных;
 * - index - индекс данных.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param cache_prefix префикс ключа кэширования или NULL;
 * \param project - имя проекта;
 * \param track - имя галса;
 * \param channel - имя канала данных;
 * \param quality - качество входных данных.
 *
 * \return Указатель на объект \link HyScanSeabed \endlink.
 *
 */

HYSCAN_CORE_EXPORT
HyScanSeabed    *hyscan_seabed_echosounder_new          (HyScanDB       *db,
                                                         HyScanCache    *cache,
                                                         gchar          *cache_prefix,
                                                         gchar          *project,
                                                         gchar          *track,
                                                         gchar          *channel,
                                                         gdouble         quality);

G_END_DECLS

#endif /* __HYSCAN_SEABED_ECHOSOUNDER_H__ */

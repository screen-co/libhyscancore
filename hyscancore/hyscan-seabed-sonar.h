/**
 * \file hyscan-seabed-sonar.h
 *
 * \brief Заголовочный файл класса обработки акустических данных
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSeabedSonar HyScanSeabedSonar - класс определения глубины по данным эхолота.
 *
 * В сочетании с интерфейсом класс позволяет всего лишь с помощью двух комманд узнавать глубину для заданного
 * индекса и не задумываться обо всём остальном. Публично доступна только одна команда:
 * #hyscan_seabed_sonar_new, создающая объект получения глубины.
 *  Дальнейшее общение с объектом ведется через интерфейс \link HyScanSeabed \endlink.
 */
 #ifndef __HYSCAN_SEABED_SONAR_H__
#define __HYSCAN_SEABED_SONAR_H__

#include <hyscan-seabed.h>
#include <hyscan-core-exports.h>
#include <hyscan-types.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <math.h>
#include <string.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SEABED_SONAR             (hyscan_seabed_sonar_get_type ())
#define HYSCAN_SEABED_SONAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SEABED_SONAR, HyScanSeabedSonar))
#define HYSCAN_IS_SEABED_SONAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SEABED_SONAR))
#define HYSCAN_SEABED_SONAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SEABED_SONAR, HyScanSeabedSonarClass))
#define HYSCAN_IS_SEABED_SONAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SEABED_SONAR))
#define HYSCAN_SEABED_SONAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SEABED_SONAR, HyScanSeabedSonarClass))

typedef struct _HyScanSeabedSonar HyScanSeabedSonar;
typedef struct _HyScanSeabedSonarPrivate HyScanSeabedSonarPrivate;
typedef struct _HyScanSeabedSonarClass HyScanSeabedSonarClass;

/* !!! Change GObject to type of the base class. !!! */
struct _HyScanSeabedSonar
{
  GObject parent_instance;

  HyScanSeabedSonarPrivate *priv;
};

/* !!! Change GObjectClass to type of the base class. !!! */
struct _HyScanSeabedSonarClass
{
  GObjectClass parent_class;
};

GType hyscan_seabed_sonar_get_type (void);

/**
 *
 * Функция создаёт новый объект определения глубины по ГБО.
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
 * \param cache_prefix префикс ключа кэширования или NULL.
 * \param project - имя проекта
 * \param track - имя галса
 * \param channel - имя канала данных
 * \param quality - качество входных данных
 * \param vsound - скорость звука. Если NULL, то берется 1500 м/с
 *
 * \return Указатель на объект \link HyScanSeabedEchosounder \endlink.
 *
 */
HYSCAN_CORE_EXPORT
HyScanSeabed * hyscan_seabed_sonar_new        (HyScanDB       *db,
                                               HyScanCache    *cache,
                                               gchar          *cache_prefix,
                                               gchar          *project,
                                               gchar          *track,
                                               gchar          *channel,
                                               gdouble         quality);

G_END_DECLS


G_END_DECLS

#endif /* __HYSCAN_SEABED_SONAR_H__ */

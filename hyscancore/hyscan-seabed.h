/**
 * \file hyscan-seabed.h
 *
 * \brief Заголовочный файл интерфейса для определения глубины
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2015
 * \license WTFPL 2.0
 *
 * \defgroup HyScanSeabed HyScanSeabed - семейство классов для определения глубины
 *
 * Интерфейс HyScanSeabed предоставляет унифицированный доступ к методу определения глубины #hyscan_seabed_get_depth_by_index.
 * Также интерфейс содержит функцию #hyscan_seabed_set_soundspeed, позволяющую задать профиль скорости звука.
 * Таким образом, пользователю нет необходимости задумываться об источниках данных и алгоритмах определения глубины
 * При этом интерфейс не предоставляет возможностей выбирать источник данных, поскольку это
 * должно происходить в момент создания экземпляра класса.
 * Интерфейс возвращает значение глубины типа gdouble.
 */

#ifndef __HYSCAN_SEABED_H__
#define __HYSCAN_SEABED_H__

#include <glib-object.h>
#include <hyscan-types.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SEABED            (hyscan_seabed_get_type ())
#define HYSCAN_SEABED(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SEABED, HyScanSeabed))
#define HYSCAN_IS_SEABED(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SEABED))
#define HYSCAN_SEABED_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SEABED, HyScanSeabedInterface))

typedef struct
{
  gdouble depth;
  gdouble soundspeed;
}SoundSpeedTable;

typedef struct _HyScanSeabed HyScanSeabed;
typedef struct _HyScanSeabedInterface HyScanSeabedInterface;

struct _HyScanSeabedInterface
{
  GTypeInterface       g_iface;

  gdouble        (*get_depth_by_index)   (HyScanSeabed *seabed,
                                          gint32        index);

  void           (*set_soundspeed)       (HyScanSeabed *seabed,
                                          GArray       *soundspeedtable);
};

HYSCAN_CORE_EXPORT
GType           hyscan_seabed_get_type      (void);

/**
 *
 * Функция возвращает значение глубины для указанного индекса
 *
 * \param seabed указатель на объект \link HyScanSeabedEchosounder \endlink или \link HyScanSeabedSonar \endlink, c которым в данный момент ведется работа;
 * \param index индекс
 * \return gdouble значение глубины
 *
 */
HYSCAN_CORE_EXPORT
gdouble         hyscan_seabed_get_depth_by_index                (HyScanSeabed *seabed,
                                                                 gint32        index);

/**
 *
 * Функция позволяет задать профиль скорости звука
 *
 * \param seabed указатель на объект \link HyScanSeabedEchosounder \endlink или \link HyScanSeabedSonar \endlink, c которым в данный момент ведется работа;
 * \param soundspeedtable указатель на GArray, состоящий из элементов типа SoundSpeedTable
 *
 */
HYSCAN_CORE_EXPORT
void            hyscan_seabed_set_soundspeed                    (HyScanSeabed *seabed,
                                                                 GArray       *soundspeedtable);
G_END_DECLS

#endif /* __HYSCAN_SEABED_H__ */

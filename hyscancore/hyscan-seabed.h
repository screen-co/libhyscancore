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
 * Под профилем скорости звука понимается таблично заданная функция скорости звука от глубины, например, такая:
 * Глубина | Скорость звука
 * :------ | :------------:
 * 0 | 1500
 * 2 | 1450
 * 4 | 1400
 * Где глубина задается в метрах, а скорость звука в метрах в секунду.
 *
 * Данный пример говорит о следующем: на глубинах от 0 до 2 метров скорость звука составляет 1500 м/с,
 * от 2 до 4 метров - 1450 м/с, от 4 и далее - 1400 м/с. Глубина обязательно должна начинаться от нуля.
 * Таблица представляет собой GArray, состоящий из структур \link SoundSpeedTable \endlink
 * Используемые объекты автоматически переведут значения глубины из метров в дискреты, в зависимости от частоты дискретизации.
 *
 * Интерфейс возвращает значение глубины типа gdouble.
 */

#ifndef __HYSCAN_SEABED_H__
#define __HYSCAN_SEABED_H__

#include <glib-object.h>
#include <hyscan-data.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SEABED            (hyscan_seabed_get_type ())
#define HYSCAN_SEABED(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SEABED, HyScanSeabed))
#define HYSCAN_IS_SEABED(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SEABED))
#define HYSCAN_SEABED_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SEABED, HyScanSeabedInterface))

/** \brief Элемент таблицы профиля скорости звука */
typedef struct
{
  gdouble depth;      /**< Глубина в м. */
  gdouble soundspeed; /**< Скорость звука в м/с. */
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
GType           hyscan_seabed_get_type               (void);

/**
 *
 * Функция возвращает значение глубины для указанного индекса
 *
 * \param seabed указатель на интерфейс \link HyScanSeabed \endlink, c которым в данный момент ведется работа;
 * \param index индекс
 * \return gdouble значение глубины
 *
 */
HYSCAN_CORE_EXPORT
gdouble         hyscan_seabed_get_depth_by_index     (HyScanSeabed *seabed,
                                                      gint32        index);

/**
 *
 * Функция позволяет задать профиль скорости звука
 *
 * \param seabed указатель на интерфейс \link HyScanSeabed \endlink, c которым в данный момент ведется работа;
 * \param soundspeedtable указатель на GArray, состоящий из элементов типа SoundSpeedTable
 *
 */
HYSCAN_CORE_EXPORT
void            hyscan_seabed_set_soundspeed         (HyScanSeabed *seabed,
                                                      GArray       *soundspeedtable);
G_END_DECLS

#endif /* __HYSCAN_SEABED_H__ */

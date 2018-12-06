/* hyscan-forward-look-data.h
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
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

/**
 * HyScanForwardLookDOA:
 * @angle: азимут цели относительно перпендикуляра к антенне, рад
 * @distance: дистанция до цели, метры
 * @amplitude: амплитуда отражённого сигнала
 *
 * Точка цели вперёдсмотрящего локатора (Direction Of Arrival)
 */
typedef struct
{
  gfloat angle;
  gfloat distance;
  gfloat amplitude;
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
GType                          hyscan_forward_look_data_get_type           (void);

HYSCAN_API
HyScanForwardLookData         *hyscan_forward_look_data_new                (HyScanDB              *db,
                                                                            HyScanCache           *cache,
                                                                            const gchar           *project_name,
                                                                            const gchar           *track_name);

HYSCAN_API
HyScanDB *                     hyscan_forward_look_data_get_db             (HyScanForwardLookData *data);

HYSCAN_API
const gchar *                  hyscan_forward_look_data_get_project_name   (HyScanForwardLookData *data);

HYSCAN_API
const gchar *                  hyscan_forward_look_data_get_track_name     (HyScanForwardLookData *data);

HYSCAN_API
HyScanAntennaPosition          hyscan_forward_look_data_get_position       (HyScanForwardLookData *data);

HYSCAN_API
gboolean                       hyscan_forward_look_data_is_writable        (HyScanForwardLookData *data);

HYSCAN_API
gdouble                        hyscan_forward_look_data_get_alpha          (HyScanForwardLookData *data);

HYSCAN_API
guint32                        hyscan_forward_look_data_get_mod_count      (HyScanForwardLookData *data);

HYSCAN_API
gboolean                       hyscan_forward_look_data_get_range          (HyScanForwardLookData *data,
                                                                            guint32               *first_index,
                                                                            guint32               *last_index);

HYSCAN_API
HyScanDBFindStatus             hyscan_forward_look_data_find_data          (HyScanForwardLookData *data,
                                                                            gint64                 time,
                                                                            guint32               *lindex,
                                                                            guint32               *rindex,
                                                                            gint64                *ltime,
                                                                            gint64                *rtime);

HYSCAN_API
void                           hyscan_forward_look_data_set_sound_velocity (HyScanForwardLookData *data,
                                                                            gdouble                sound_velocity);

HYSCAN_API
gboolean                       hyscan_forward_look_data_get_size_time      (HyScanForwardLookData *data,
                                                                            guint32                index,
                                                                            guint32               *n_points,
                                                                            gint64                *time);

HYSCAN_API
const HyScanForwardLookDOA    *hyscan_forward_look_data_get_doa            (HyScanForwardLookData *data,
                                                                            guint32                index,
                                                                            guint32               *n_points,
                                                                            gint64                *time);

G_END_DECLS

#endif /* __HYSCAN_FORWARD_LOOK_DATA_H__ */

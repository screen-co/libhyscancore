/* hyscan-tile-queue.h
 *
 * Copyright 2016-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanCore.
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

#ifndef __HYSCAN_TILE_QUEUE_H__
#define __HYSCAN_TILE_QUEUE_H__

#include <hyscan-tile-common.h>
#include <hyscan-amplitude-factory.h>
#include <hyscan-depth-factory.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TILE_QUEUE             (hyscan_tile_queue_get_type ())
#define HYSCAN_TILE_QUEUE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TILE_QUEUE, HyScanTileQueue))
#define HYSCAN_IS_TILE_QUEUE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TILE_QUEUE))
#define HYSCAN_TILE_QUEUE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TILE_QUEUE, HyScanTileQueueClass))
#define HYSCAN_IS_TILE_QUEUE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TILE_QUEUE))
#define HYSCAN_TILE_QUEUE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TILE_QUEUE, HyScanTileQueueClass))

typedef struct _HyScanTileQueue HyScanTileQueue;
typedef struct _HyScanTileQueuePrivate HyScanTileQueuePrivate;
typedef struct _HyScanTileQueueClass HyScanTileQueueClass;

struct _HyScanTileQueue
{
  GObject parent_instance;

  HyScanTileQueuePrivate *priv;
};

struct _HyScanTileQueueClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_tile_queue_get_type              (void);

HYSCAN_API
HyScanTileQueue *       hyscan_tile_queue_new                  (gint                    max_generators,
                                                                HyScanCache            *cache,
                                                                HyScanAmplitudeFactory *amp_factory,
                                                                HyScanDepthFactory     *dpt_factory);

HYSCAN_API
void                    hyscan_tile_queue_set_ship_speed       (HyScanTileQueue        *tilequeue,
                                                                gfloat                  speed);

HYSCAN_API
void                    hyscan_tile_queue_set_sound_velocity   (HyScanTileQueue        *tilequeue,
                                                                GArray                 *velocity);

HYSCAN_API
void                    hyscan_tile_queue_amp_changed          (HyScanTileQueue        *tilequeue);

HYSCAN_API
void                    hyscan_tile_queue_dpt_changed          (HyScanTileQueue        *tilequeue);

HYSCAN_API
gboolean                hyscan_tile_queue_check                (HyScanTileQueue        *tilequeue,
                                                                HyScanTile             *requested_tile,
                                                                HyScanTile             *cached_tile,
                                                                gboolean               *regenerate);

HYSCAN_API
gboolean                hyscan_tile_queue_get                  (HyScanTileQueue        *tilequeue,
                                                                HyScanTile             *requested_tile,
                                                                HyScanTile             *cached_tile,
                                                                gfloat                **buffer,
                                                                guint32                *size);

HYSCAN_API
void                    hyscan_tile_queue_add                  (HyScanTileQueue        *tilequeue,
                                                                HyScanTile             *tile);

HYSCAN_API
void                    hyscan_tile_queue_add_finished         (HyScanTileQueue        *tilequeue,
                                                                guint64                 view_id);

G_END_DECLS

#endif /* __HYSCAN_TILE_QUEUE_H__ */

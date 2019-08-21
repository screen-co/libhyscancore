/* hyscan-tile.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

/**
 * SECTION: hyscan-tile
 * @Title HyScanTile
 * @Short_description
 *
 */

#include "hyscan-tile.h"
#include <math.h>

enum
{
  PROP_0,
  PROP_TRACK
};

struct _HyScanTilePrivate
{
  gchar * track;
  gchar * token;
};

static void    hyscan_tile_set_property    (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void    hyscan_tile_object_finalize (GObject      *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTile, hyscan_tile, G_TYPE_OBJECT);

static void
hyscan_tile_class_init (HyScanTileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_tile_set_property;
  oclass->finalize = hyscan_tile_object_finalize;

  g_object_class_install_property (oclass, PROP_TRACK,
    g_param_spec_string ("track", "Track", "Track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_tile_init (HyScanTile *tile)
{
  tile->priv = hyscan_tile_get_instance_private (tile);
}

static void
hyscan_tile_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  HyScanTile *self = HYSCAN_TILE (object);
  HyScanTilePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_TRACK:
      priv->track = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_tile_object_finalize (GObject *object)
{
  HyScanTile *self = HYSCAN_TILE (object);
  HyScanTilePrivate *priv = self->priv;

  g_free (priv->track);
  g_free (priv->token);

  G_OBJECT_CLASS (hyscan_tile_parent_class)->finalize (object);
}

HyScanTile *
hyscan_tile_new (const gchar *track)
{
  return g_object_new (HYSCAN_TYPE_TILE,
                       "track", track,
                       NULL);
}

const gchar *
hyscan_tile_get_track (HyScanTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_TILE (tile), NULL);

  return tile->priv->track;
}

const gchar *
hyscan_tile_get_token (HyScanTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_TILE (tile), NULL);

  if (tile->priv->token == NULL)
    {
      tile->priv->token = g_strdup_printf("(%s|%i.%i.%i.%i.%010.3f.%06.3f|%u.%i.%i.%i)",
                                          tile->priv->track,

                                          tile->info.across_start,
                                          tile->info.along_start,
                                          tile->info.across_end,
                                          tile->info.along_end,
                                          tile->info.scale,
                                          tile->info.ppi,

                                          tile->info.upsample,
                                          tile->info.flags,
                                          tile->info.rotate,
                                          tile->info.source);

    }

  return tile->priv->token;
}

gboolean
hyscan_tile_compare (HyScanTile *tile,
                     HyScanTile *other)
{
  g_return_val_if_fail (HYSCAN_IS_TILE (tile), FALSE);
  g_return_val_if_fail (HYSCAN_IS_TILE (other), FALSE);

  if (tile->info.across_start != other->info.across_start ||
      tile->info.along_start  != other->info.along_start  ||
      tile->info.across_end   != other->info.across_end   ||
      tile->info.along_end    != other->info.along_end    ||
      tile->info.ppi          != other->info.ppi          ||
      tile->info.scale        != other->info.scale)
    {
      return FALSE;
    }

  return TRUE;
}

gfloat
hyscan_tile_common_mm_per_pixel (gfloat scale,
                                 gfloat ppi)
{
  return 25.4 * scale / ppi;
}

gint32
hyscan_tile_common_tile_size (gint32 start,
                              gint32 end,
                              gfloat step)
{
  return ceil ((end - start) / step);
}

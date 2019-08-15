/* hyscan-factory-depth.c
 *
 * Copyright 2018-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-factory-depth
 * @Short_description: фабрика объектов доступа к данным глубины
 * @Title: HyScanFactoryDepth
 * @see_also: HyScanDepthometer
 *
 * Объект является фабрикой объектов доступа к данным глубины.
 */


#include "hyscan-factory-depth.h"
#include <hyscan-nmea-parser.h>
#include <string.h>

enum
{
  PROP_0,
  PROP_CACHE,
};

struct _HyScanFactoryDepthPrivate
{
  HyScanCache  *cache;

  HyScanDB     *db;
  gchar        *project; /* */
  gchar        *track;

  GMutex        lock;
  gchar        *token;
};

static void    hyscan_factory_depth_set_property             (GObject               *object,
                                                              guint                  prop_id,
                                                              const GValue          *value,
                                                              GParamSpec            *pspec);
static void    hyscan_factory_depth_object_constructed       (GObject               *object);
static void    hyscan_factory_depth_object_finalize          (GObject               *object);
static void    hyscan_factory_depth_updated                  (HyScanFactoryDepth    *self);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanFactoryDepth, hyscan_factory_depth, HYSCAN_TYPE_FACTORY_MOTHER);

static void
hyscan_factory_depth_class_init (HyScanFactoryDepthClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_factory_depth_set_property;
  oclass->constructed = hyscan_factory_depth_object_constructed;
  oclass->finalize = hyscan_factory_depth_object_finalize;

  g_object_class_install_property (oclass, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_factory_depth_init (HyScanFactoryDepth *factory_depth)
{
  factory_depth->priv = hyscan_factory_depth_get_instance_private (factory_depth);
}

static void
hyscan_factory_depth_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanFactoryDepth *factory_depth = HYSCAN_FACTORY_DEPTH (object);
  HyScanFactoryDepthPrivate *priv = factory_depth->priv;

  switch (prop_id)
    {
    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_factory_depth_object_constructed (GObject *object)
{
  HyScanFactoryDepth *factory_depth = HYSCAN_FACTORY_DEPTH (object);
  HyScanFactoryDepthPrivate *priv = factory_depth->priv;

  g_mutex_init (&priv->lock);
}

static void
hyscan_factory_depth_object_finalize (GObject *object)
{
  HyScanFactoryDepth *factory_depth = HYSCAN_FACTORY_DEPTH (object);
  HyScanFactoryDepthPrivate *priv = factory_depth->priv;

  g_clear_object (&priv->cache);

  g_clear_object (&priv->db);
  g_free (priv->project);
  g_free (priv->track);

  g_free (priv->token);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_factory_depth_parent_class)->finalize (object);
}

/* Функция обновляет состояние объекта. Вызывать строго за мьютексом! */
static void
hyscan_factory_depth_updated (HyScanFactoryDepth *self)
{
  HyScanFactoryDepthPrivate *priv = self->priv;
  gchar *uri = NULL;

  g_clear_pointer (&priv->token, g_free);

  if (priv->db == NULL || priv->project == NULL || priv->track == NULL)
    return;

  uri = hyscan_db_get_uri (priv->db);

  priv->token = g_strdup_printf ("FactoryDepth.%s.%s.%s",
                                 uri, priv->project, priv->track);

  g_free (uri);
}

/**
 * hyscan_factory_depth_new:
 * @cache: (nullable): объект #HyScanCache
 *
 * Создает новый объект #HyScanFactoryDepth.
 *
 * Returns: (transfer full): #HyScanFactoryDepth
 */
HyScanFactoryDepth *
hyscan_factory_depth_new (HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_FACTORY_DEPTH,
                       "cache", cache,
                       NULL);
}

/**
 * hyscan_factory_depth_get_token:
 * @self: объект #HyScanDepthfactory
 *
 * Функция возвращает токен объекта (строка, описывающее его внутреннее состояние).
 *
 * Returns: (transfer full): токен.
 */
gchar *
hyscan_factory_depth_get_token (HyScanFactoryDepth *self)
{
  HyScanFactoryDepthPrivate *priv;
  gchar *token = NULL;

  g_return_val_if_fail (HYSCAN_IS_FACTORY_DEPTH (self), NULL);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  if (priv->token != NULL)
    token = g_strdup (priv->token);
  g_mutex_unlock (&priv->lock);

  return token;
}

/**
 * hyscan_factory_depth_set_track:
 * @self: объект #HyScanDepthfactory
 * @db: база данных
 * @project: проект
 * @track: галс
 *
 * Функция задает БД, проект и галс.
 */
void
hyscan_factory_depth_set_track (HyScanFactoryDepth *self,
                                HyScanDB           *db,
                                const gchar        *project,
                                const gchar        *track)
{
  HyScanFactoryDepthPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FACTORY_DEPTH (self));
  priv = self->priv;

  g_mutex_lock (&priv->lock);

  g_clear_object (&priv->db);
  g_clear_pointer (&priv->project, g_free);
  g_clear_pointer (&priv->track, g_free);

  priv->db = g_object_ref (db);
  priv->project = g_strdup (project);
  priv->track = g_strdup (track);

  hyscan_factory_depth_updated (self);

  g_mutex_unlock (&priv->lock);

  hyscan_factory_mother_emit_changed (HYSCAN_FACTORY_MOTHER (self));
}

/**
 * hyscan_factory_depth_produce:
 * @self: объект #HyScanDepthfactory
 *
 * Функция создает новый объект доступа к данным глубины.
 *
 * Returns: (transfer full): #HyScanDepthometer
 */
HyScanDepthometer *
hyscan_factory_depth_produce (HyScanFactoryDepth *self)
{
  HyScanNMEAParser *parser = NULL;
  HyScanDepthometer *depth = NULL;
  HyScanFactoryDepthPrivate *priv;
  HyScanDB * db;
  gchar * project;
  gchar * track;

  g_return_val_if_fail (HYSCAN_IS_FACTORY_DEPTH (self), NULL);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  db = (priv->db != NULL) ? g_object_ref (priv->db) : NULL;
  project = (priv->project != NULL) ? g_strdup (priv->project) : NULL;
  track = (priv->track != NULL) ? g_strdup (priv->track) : NULL;
  g_mutex_unlock (&priv->lock);

  if (db == NULL || project == NULL || track == NULL)
    goto fail;

  parser = hyscan_nmea_parser_new (priv->db, priv->cache,
                                   priv->project, priv->track, 1,
                                   HYSCAN_NMEA_DATA_DPT,
                                   HYSCAN_NMEA_FIELD_DEPTH);

  if (parser == NULL)
    goto fail;

  depth = hyscan_depthometer_new (HYSCAN_NAV_DATA (parser), priv->cache);

fail:
  g_clear_object (&db);
  g_clear_object (&parser);
  g_clear_pointer (&project, g_free);
  g_clear_pointer (&track, g_free);

  return depth;
}

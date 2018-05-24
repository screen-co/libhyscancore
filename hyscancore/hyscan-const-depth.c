#include "hyscan-const-depth.h"
#include <hyscan-projector.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_RAW
};

struct _HyScanConstDepthPrivate
{
  HyScanDB             *db;        /* БД. */
  gchar                *project;   /* Проект. */
  gchar                *track;     /* Галс. */
  HyScanSourceType      source;    /* Тип источника. */
  gboolean              raw;       /* Сырые данные. */

  HyScanProjector      *projector; /* Проецирование. */
  HyScanAcousticData   *dc;        /* КД. */

  gchar                *db_uri;    /* Путь к БД. */
  gchar                *token;     /* Токен объекта. */

  gfloat                distance;  /* Количество глубин в зондировании. */
};

static void    hyscan_const_depth_interface_init           (HyScanNavDataInterface *iface);
static void    hyscan_const_depth_set_property             (GObject                *object,
                                                            guint                   prop_id,
                                                            const GValue           *value,
                                                            GParamSpec             *pspec);
static void    hyscan_const_depth_object_constructed       (GObject                *object);
static void    hyscan_const_depth_object_finalize          (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanConstDepth, hyscan_const_depth, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanConstDepth)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_NAV_DATA, hyscan_const_depth_interface_init));

static void
hyscan_const_depth_class_init (HyScanConstDepthClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_const_depth_set_property;

  object_class->constructed = hyscan_const_depth_object_constructed;
  object_class->finalize = hyscan_const_depth_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_TYPE,
    g_param_spec_int ("source-type", "SourceType", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_RAW,
    g_param_spec_boolean ("raw", "Raw", "Use raw data type", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_const_depth_init (HyScanConstDepth *const_depth)
{
  const_depth->priv = hyscan_const_depth_get_instance_private (const_depth);
}

static void
hyscan_const_depth_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanConstDepth *const_depth = HYSCAN_CONST_DEPTH (object);
  HyScanConstDepthPrivate *priv = const_depth->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track = g_value_dup_string (value);
      break;

    case PROP_SOURCE_TYPE:
      priv->source = g_value_get_int (value);
      break;

    case PROP_RAW:
      priv->raw = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_const_depth_object_constructed (GObject *object)
{
  HyScanConstDepth *const_depth = HYSCAN_CONST_DEPTH (object);
  HyScanConstDepthPrivate *priv = const_depth->priv;
  const HyScanAcousticData *dc;
  gchar *db_uri;

  if (priv->db == NULL || priv->project == NULL || priv->track == NULL)
    return;

  priv->projector = hyscan_projector_new (priv->db, priv->project, priv->track,
                                          priv->source, priv->raw);
  if (priv->projector == NULL)
    return;

  /* Здесь требуется выполнить преобразование типов. */
  dc = hyscan_projector_get_acoustic_data (priv->projector);
  priv->dc = (HyScanAcousticData*) dc;

  db_uri = hyscan_db_get_uri (priv->db);
  priv->token = g_strdup_printf ("const_depth.%s.%s.%s.%i.%i",
                                 db_uri, priv->project, priv->track,
                                 priv->source, priv->raw);
  g_free (db_uri);
}

static void
hyscan_const_depth_object_finalize (GObject *object)
{
  HyScanConstDepth *const_depth = HYSCAN_CONST_DEPTH (object);
  HyScanConstDepthPrivate *priv = const_depth->priv;

  g_clear_object (&priv->db);
  g_free (priv->project);
  g_free (priv->track);
  g_clear_object (&priv->projector);

  G_OBJECT_CLASS (hyscan_const_depth_parent_class)->finalize (object);
}

HyScanConstDepth*
hyscan_const_depth_new (HyScanDB         *db,
                        const gchar      *project,
                        const gchar      *track,
                        HyScanSourceType  source_type,
                        gboolean          raw)
{
  HyScanConstDepth *cdepth;

  cdepth = g_object_new (HYSCAN_TYPE_CONST_DEPTH,
                         "db", db,
                         "project-name", project,
                         "track-name", track,
                         "source-type", source_type,
                         "raw", raw,
                         NULL);

  if (cdepth->priv->projector == NULL)
    g_clear_object (&cdepth);

  return cdepth;
}

void
hyscan_const_depth_set_distance (HyScanConstDepth *self,
                                 gfloat            distance)
{
  HyScanConstDepthPrivate *priv = self->priv;
  g_return_if_fail (HYSCAN_IS_CONST_DEPTH (self));

  priv->distance = distance;
}

/* Функция установки кэша. */
static void
hyscan_const_depth_set_cache (HyScanNavData *ndata,
                              HyScanCache   *cache)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  HyScanConstDepthPrivate *priv = self->priv;

  hyscan_projector_set_cache (priv->projector, cache);
}

/* Функция получения глубины. */
static gboolean
hyscan_const_depth_get (HyScanNavData *ndata,
                        guint32        index,
                        gint64        *time,
                        gdouble       *value)
{
  guint32 n_vals;
  gdouble depth;

  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  HyScanConstDepthPrivate *priv = self->priv;

  if (NULL == hyscan_acoustic_data_get_values (priv->dc, index, &n_vals, time))
    return FALSE;

  depth = n_vals / priv->distance;

  if (hyscan_projector_count_to_coord (priv->projector, depth, value, 0.0))
    return TRUE;

  return FALSE;
}

/* Функция поиска данных. */
static HyScanDBFindStatus
hyscan_const_depth_find_data (HyScanNavData *ndata,
                              gint64       time,
                              guint32     *lindex,
                              guint32     *rindex,
                              gint64      *ltime,
                              gint64      *rtime)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  return hyscan_acoustic_data_find_data (self->priv->dc, time,
                                         lindex, rindex,
                                         ltime, rtime);
}

/* Функция определения диапазона. */
static gboolean
hyscan_const_depth_get_range (HyScanNavData *ndata,
                              guint32     *first,
                              guint32     *last)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  return hyscan_acoustic_data_get_range (self->priv->dc, first, last);
}

/* Функция получения местоположения антенны. */
static HyScanAntennaPosition
hyscan_const_depth_get_position (HyScanNavData *ndata)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  return hyscan_acoustic_data_get_position (self->priv->dc);
}

/* Функция определяет, возможна ли дозапись в канал данных. */
static gboolean
hyscan_const_depth_is_writable (HyScanNavData *ndata)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  return hyscan_acoustic_data_is_writable (self->priv->dc);
}

/* Функция возвращает токен объекта. */
static const gchar*
hyscan_const_depth_get_token (HyScanNavData *ndata)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  return self->priv->token;

}

/* Функция возвращает значение счётчика изменений. */
static guint32
hyscan_const_depth_get_mod_count (HyScanNavData *ndata)
{
  HyScanConstDepth *self = HYSCAN_CONST_DEPTH (ndata);
  return hyscan_acoustic_data_get_mod_count (self->priv->dc);
}


static void
hyscan_const_depth_interface_init (HyScanNavDataInterface *iface)
{
  iface->set_cache     = hyscan_const_depth_set_cache;
  iface->get           = hyscan_const_depth_get;
  iface->find_data     = hyscan_const_depth_find_data;
  iface->get_range     = hyscan_const_depth_get_range;
  iface->get_position  = hyscan_const_depth_get_position;
  iface->get_token     = hyscan_const_depth_get_token;
  iface->is_writable   = hyscan_const_depth_is_writable;
  iface->get_mod_count = hyscan_const_depth_get_mod_count;
}

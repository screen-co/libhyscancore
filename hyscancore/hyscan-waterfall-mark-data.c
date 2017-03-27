#include "hyscan-waterfall-mark-data.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define MARK_ID_LEN 20

enum
{
  PROP_0,
  PROP_DB,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_PROFILE
};

struct _HyScanWaterfallMarkDataPrivate
{
  HyScanDB          *db;       /* Интерфейс БД. */
  gchar             *project;  /* Проект. */
  gchar             *track;    /* Галс. */
  gchar             *profile;  /* Профиль обработки данных. */

  gint32             param_id; /* Идентификатор группы параметров. */

  GRand             *rand;     /* Генератор случайных чисел. */
};

static void    hyscan_waterfall_mark_data_set_property            (GObject               *object,
                                                                   guint                  prop_id,
                                                                   const GValue          *value,
                                                                   GParamSpec            *pspec);
static void    hyscan_waterfall_mark_data_object_constructed      (GObject               *object);
static void    hyscan_waterfall_mark_data_object_finalize         (GObject               *object);

static gchar  *hyscan_waterfall_mark_data_generate_id             (GRand                 *rand);

static gboolean hyscan_waterfall_mark_data_get_internal           (HyScanWaterfallMarkDataPrivate *priv,
                                                                   const gchar           *id,
                                                                   gchar                **name,
                                                                   gchar                **description,
                                                                   gchar                **operator_name,
                                                                   guint64               *labels,
                                                                   gint64                *creation_time,
                                                                   gint64                *modification_time,
                                                                   HyScanSourceType      *source0,
                                                                   guint32               *index0,
                                                                   guint32               *count0,
                                                                   HyScanSourceType      *source1,
                                                                   guint32               *index1,
                                                                   guint32               *count1);
static gboolean hyscan_waterfall_mark_data_set_internal           (HyScanWaterfallMarkDataPrivate *priv,
                                                                   const gchar           *id,
                                                                   const gchar           *name,
                                                                   const gchar           *description,
                                                                   const gchar           *operator_name,
                                                                   guint64                labels,
                                                                   gint64                 creation_time,
                                                                   gint64                 modification_time,
                                                                   HyScanSourceType       source0,
                                                                   guint32                index0,
                                                                   guint32                count0,
                                                                   HyScanSourceType       source1,
                                                                   guint32                index1,
                                                                   guint32                count1);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanWaterfallMarkData, hyscan_waterfall_mark_data, G_TYPE_OBJECT);

static void
hyscan_waterfall_mark_data_class_init (HyScanWaterfallMarkDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_waterfall_mark_data_set_property;

  object_class->constructed = hyscan_waterfall_mark_data_object_constructed;
  object_class->finalize = hyscan_waterfall_mark_data_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "Project name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track", "TrackName", "Track name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROFILE,
      g_param_spec_string ("profile", "ProfileName", "Profile name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_waterfall_mark_data_init (HyScanWaterfallMarkData *self)
{
  self->priv = hyscan_waterfall_mark_data_get_instance_private (self);
}

static void
hyscan_waterfall_mark_data_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanWaterfallMarkData *self = HYSCAN_WATERFALL_MARK_DATA (object);
  HyScanWaterfallMarkDataPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK:
      priv->track = g_value_dup_string (value);
      break;

    case PROP_PROFILE:
      priv->profile = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_waterfall_mark_data_object_constructed (GObject *object)
{
  gint32 project_id = 0;      /* Идентификатор проекта. */
  gint32 track_id = 0;        /* Идентификатор галса. */
  gint32 track_param_id = 0;  /* Группа параметров галса. */
  gchar *track_id_str = NULL; /* Идентификатор трека. */
  gchar *group_name = NULL;   /* Имя создаваемой/открываемой группы параметров. */

  HyScanWaterfallMarkData *self = HYSCAN_WATERFALL_MARK_DATA (object);
  HyScanWaterfallMarkDataPrivate *priv = self->priv;

  if (priv->db == NULL)
    {
      g_warning ("HyScanWaterfallMarkData: db not specified");
      goto exit;
    }

  /* Открываем проект. */
  project_id = hyscan_db_project_open (priv->db, priv->project);
  if (project_id <= 0)
    {
      g_warning ("HyScanWaterfallMarkData: can't open project '%s'", priv->project);
      goto exit;
    }

  /* Также необходимо знать идентификатор трека. */
  track_id = hyscan_db_track_open (priv->db, project_id, priv->track);
  if (track_id <= 0)
    {
      g_warning ("HyScanWaterfallMarkData: can't open track %s (project '%s')",
                 priv->track, priv->project);
      goto exit;
    }

  track_param_id = hyscan_db_track_param_open (priv->db, track_id);
  if (track_param_id <= 0)
    {
      g_warning ("HyScanWaterfallMarkData: can't open track %s parameters (project '%s')",
                 priv->track, priv->project);
      goto exit;
    }

  track_id_str = hyscan_db_param_get_string (priv->db, track_param_id, NULL, "/id");

  /* Составляем название группы параметров и заодно имя оператора.*/
  if (priv->profile == NULL)
    group_name = g_strdup_printf ("waterfall-marks-%s-default", track_id_str);
  else
    group_name = g_strdup_printf ("waterfall-marks-%s-%s", track_id_str, priv->profile);

  /* Открываем (или создаем) группу параметров. */
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, group_name);
  if (priv->param_id <= 0)
    {
      g_warning ("HyScanWaterfallMarkData: can't open group %s (project '%s')",
                 group_name, priv->project);
      goto exit;
    }

  priv->rand = g_rand_new ();

exit:
  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);
  if (track_param_id > 0)
    hyscan_db_close (priv->db, track_param_id);

  g_free (group_name);
  g_free (track_id_str);
}

static void
hyscan_waterfall_mark_data_object_finalize (GObject *object)
{
  HyScanWaterfallMarkData *self = HYSCAN_WATERFALL_MARK_DATA (object);
  HyScanWaterfallMarkDataPrivate *priv = self->priv;

  if (priv->rand != NULL)
    g_rand_free (priv->rand);

  g_free (priv->project);
  g_free (priv->track);
  g_free (priv->profile);

  if (priv->param_id > 0)
    hyscan_db_close (priv->db, priv->param_id);

  g_object_unref (priv->db);

  G_OBJECT_CLASS (hyscan_waterfall_mark_data_parent_class)->finalize (object);
}

static gchar*
hyscan_waterfall_mark_data_generate_id (GRand *rand)
{
  guint i;
  gchar *id;
  id = g_malloc (MARK_ID_LEN + 1);

  id[MARK_ID_LEN] = '\0';
  for (i = 0; i < MARK_ID_LEN; i++)
    {
      gint rnd = g_rand_int_range (rand, 0, 62);
      if (rnd < 10)
        id[i] = '0' + rnd;
      else if (rnd < 36)
        id[i] = 'a' + rnd - 10;
      else
        id[i] = 'A' + rnd - 36;
    }

  return id;
}

/* Функция считывает содержимое объекта. */
static gboolean
hyscan_waterfall_mark_data_get_internal (HyScanWaterfallMarkDataPrivate *priv,
                                         const gchar           *id,
                                         gchar                **name,
                                         gchar                **description,
                                         gchar                **operator_name,
                                         guint64               *labels,
                                         gint64                *creation_time,
                                         gint64                *modification_time,
                                         HyScanSourceType      *source0,
                                         guint32               *index0,
                                         guint32               *count0,
                                         HyScanSourceType      *source1,
                                         guint32               *index1,
                                         guint32               *count1)
{
  const gchar *param_names[15];
  GVariant *param_values[15];
  gboolean status = FALSE;
  gint i;

  param_names[ 0] = "/schema/id";
  param_names[ 1] = "/schema/version";
  param_names[ 2] = "/name";
  param_names[ 3] = "/description";
  param_names[ 4] = "/label";
  param_names[ 5] = "/operator";
  param_names[ 6] = "/time/creation";
  param_names[ 7] = "/time/modification";
  param_names[ 8] = "/coordinates/source0";
  param_names[ 9] = "/coordinates/index0";
  param_names[10] = "/coordinates/count0";
  param_names[11] = "/coordinates/source1";
  param_names[12] = "/coordinates/index1";
  param_names[13] = "/coordinates/count1";
  param_names[14] = NULL;

  if (!hyscan_db_param_get (priv->db, priv->param_id, id, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) != WATERFALL_MARK_SCHEMA_ID) ||
      (g_variant_get_int64 (param_values[1]) != WATERFALL_MARK_SCHEMA_VERSION))
    {
      goto exit;
    }

  /* Метки считаны успешно. */
  status = TRUE;

  name              != NULL ? *name              = g_strdup (g_variant_get_string (param_values[2], NULL)) : 0;
  description       != NULL ? *description       = g_strdup (g_variant_get_string (param_values[3], NULL)) : 0;
  labels            != NULL ? *labels            = g_variant_get_int64  (param_values[4]) : 0;
  operator_name     != NULL ? *operator_name     = g_strdup (g_variant_get_string (param_values[5], NULL)) : 0;
  creation_time     != NULL ? *creation_time     = g_variant_get_int64  (param_values[6]) : 0;
  modification_time != NULL ? *modification_time = g_variant_get_int64  (param_values[7]) : 0;
  source0           != NULL ? *source0           = g_variant_get_int64  (param_values[8]) : 0;
  index0            != NULL ? *index0            = g_variant_get_int64  (param_values[9]) : 0;
  count0            != NULL ? *count0            = g_variant_get_int64  (param_values[10]) : 0;
  source1           != NULL ? *source1           = g_variant_get_int64  (param_values[11]) : 0;
  index1            != NULL ? *index1            = g_variant_get_int64  (param_values[12]) : 0;
  count1            != NULL ? *count1            = g_variant_get_int64  (param_values[13]) : 0;


exit:
  for (i = 0; i < 14; i++)
    g_variant_unref (param_values[i]);

  return status;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_waterfall_mark_data_set_internal (HyScanWaterfallMarkDataPrivate *priv,
                                         const gchar         *id,
                                         const gchar         *name,
                                         const gchar         *description,
                                         const gchar         *operator_name,
                                         guint64              labels,
                                         gint64               creation_time,
                                         gint64               modification_time,
                                         HyScanSourceType     source0,
                                         guint32              index0,
                                         guint32              count0,
                                         HyScanSourceType     source1,
                                         guint32              index1,
                                         guint32              count1)
{
  const gchar *param_names[13];
  GVariant *param_values[13];
  gint i;

  param_names[0] = "/name";
  param_names[1] = "/description";
  param_names[2] = "/label";
  param_names[3] = "/operator";
  param_names[4] = "/time/creation";
  param_names[5] = "/time/modification";
  param_names[6] = "/coordinates/source0";
  param_names[7] = "/coordinates/index0";
  param_names[8] = "/coordinates/count0";
  param_names[9] = "/coordinates/source1";
  param_names[10] = "/coordinates/index1";
  param_names[11] = "/coordinates/count1";
  param_names[12] = NULL;

  param_values[0] = g_variant_new_string (name);
  param_values[1] = g_variant_new_string (description);
  param_values[2] = g_variant_new_int64  (labels);
  param_values[3] = g_variant_new_string (operator_name);
  param_values[4] = g_variant_new_int64  (creation_time);
  param_values[5] = g_variant_new_int64  (modification_time);
  param_values[6] = g_variant_new_int64  (source0);
  param_values[7] = g_variant_new_int64  (index0);
  param_values[8] = g_variant_new_int64  (count0);
  param_values[9] = g_variant_new_int64  (source1);
  param_values[10] = g_variant_new_int64  (index1);
  param_values[11] = g_variant_new_int64  (count1);

  if (hyscan_db_param_set (priv->db, priv->param_id, id, param_names, param_values))
    return TRUE;

  for (i = 0; i < 12; i++)
    g_variant_unref (param_values[i]);

  return FALSE;
}

HyScanWaterfallMarkData*
hyscan_waterfall_mark_data_new (HyScanDB    *db,
                                const gchar *project,
                                const gchar *track,
                                const gchar *profile)
{
  HyScanWaterfallMarkData* new;
  new = g_object_new (HYSCAN_TYPE_WATERFALL_MARK_DATA,
                      "db",      db,
                      "project", project,
                      "track",   track,
                      "profile", profile,
                      NULL);

  if (new->priv->param_id <= 0)
    g_clear_object (&new);

  return new;
}

gboolean
hyscan_waterfall_mark_data_add (HyScanWaterfallMarkData *self,
                                HyScanWaterfallMark     *mark)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), FALSE);
  return hyscan_waterfall_mark_data_add_full (self,
                                              mark->name,
                                              mark->description,
                                              mark->operator_name,
                                              mark->labels,
                                              mark->creation_time,
                                              mark->modification_time,
                                              mark->source0,
                                              mark->index0,
                                              mark->count0,
                                              mark->source1,
                                              mark->index1,
                                              mark->count1);
}

gboolean
hyscan_waterfall_mark_data_add_full (HyScanWaterfallMarkData *self,
                                     gchar                   *name,
                                     gchar                   *description,
                                     gchar                   *operator_name,
                                     guint64                  labels,
                                     gint64                   creation_time,
                                     gint64                   modification_time,
                                     HyScanSourceType         source0,
                                     guint32                  index0,
                                     guint32                  count0,
                                     HyScanSourceType         source1,
                                     guint32                  index1,
                                     guint32                  count1)
{
  gchar *id;
  gboolean status;
  HyScanWaterfallMarkDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), FALSE);
  priv = self->priv;

  id = hyscan_waterfall_mark_data_generate_id (priv->rand);

  status = hyscan_db_param_object_create (priv->db, priv->param_id,
                                          id, WATERFALL_MARK_SCHEMA);
  if (!status)
    {
      g_info ("Failed to create object %s", id);
      goto exit;
    }

  status = hyscan_waterfall_mark_data_set_internal (priv, id, name, description,
                                                    operator_name, labels,
                                                    creation_time,
                                                    modification_time,
                                                    source0, index0, count0,
                                                    source1, index1, count1);

exit:
  g_free (id);
  return status;
}

gboolean
hyscan_waterfall_mark_data_remove (HyScanWaterfallMarkData *self,
                                   const gchar             *id)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), FALSE);

  return hyscan_db_param_object_remove (self->priv->db,
                                        self->priv->param_id,
                                        id);
}

gboolean
hyscan_waterfall_mark_data_modify (HyScanWaterfallMarkData *self,
                                   const gchar             *id,
                                   HyScanWaterfallMark     *mark)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), FALSE);

  return hyscan_waterfall_mark_data_modify_full (self, id, mark->name,
                                                 mark->description, mark->operator_name,
                                                 mark->labels, mark->creation_time,
                                                 mark->modification_time,
                                                 mark->source0, mark->index0, mark->count0,
                                                 mark->source1, mark->index1, mark->count1);
}

gboolean
hyscan_waterfall_mark_data_modify_full (HyScanWaterfallMarkData    *self,
                                        const gchar                *id,
                                        gchar                      *name,
                                        gchar                      *description,
                                        gchar                      *operator_name,
                                        guint64                     labels,
                                        gint64                      creation_time,
                                        gint64                      modification_time,
                                        HyScanSourceType            source0,
                                        guint32                     index0,
                                        guint32                     count0,
                                        HyScanSourceType            source1,
                                        guint32                     index1,
                                        guint32                     count1)
{
  /* Проверяем, что метка существует. */
  if (!hyscan_waterfall_mark_data_get_internal (self->priv, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
    return FALSE;

  return hyscan_waterfall_mark_data_set_internal (self->priv, id, name, description, operator_name, labels,
                                                  creation_time, modification_time,
                                                  source0, index0, count0,
                                                  source1, index1, count1);
}

gchar**
hyscan_waterfall_mark_data_get_ids (HyScanWaterfallMarkData *self,
                                    guint                   *len)
{
  gchar** objects;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), NULL);

  objects = hyscan_db_param_object_list (self->priv->db, self->priv->param_id);

  if (len == NULL)
    return objects;

  /* if (len != NULL) */
  *len = (objects != NULL) ? g_strv_length (objects) : 0;

  return objects;
}

HyScanWaterfallMark*
hyscan_waterfall_mark_data_get (HyScanWaterfallMarkData *self,
                                const gchar             *id)
{
  gboolean status;
  HyScanWaterfallMark *mark;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), FALSE);

  mark = g_new0 (HyScanWaterfallMark, 1);

  status = hyscan_waterfall_mark_data_get_internal (self->priv, id,
                                                    &mark->name,
                                                    &mark->description,
                                                    &mark->operator_name,
                                                    &mark->labels,
                                                    &mark->creation_time,
                                                    &mark->modification_time,
                                                    &mark->source0,
                                                    &mark->index0,
                                                    &mark->count0,
                                                    &mark->source1,
                                                    &mark->index1,
                                                    &mark->count1);
  if (status)
    return mark;

  g_free (mark);
  return NULL;
}

gboolean
hyscan_waterfall_mark_data_get_full (HyScanWaterfallMarkData *self,
                                     const gchar             *id,
                                     HyScanWaterfallMark     *mark)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), FALSE);

  /* Вдруг пользователю просто интересно, есть ли такая метка. */
  if (mark == NULL)
    return hyscan_waterfall_mark_data_get_internal (self->priv, id, NULL, NULL, NULL,
                                                    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

  /* Иначе возвращаем всю метку целиком. */
  return hyscan_waterfall_mark_data_get_internal (self->priv, id,
                                                  &mark->name,
                                                  &mark->description,
                                                  &mark->operator_name,
                                                  &mark->labels,
                                                  &mark->creation_time,
                                                  &mark->modification_time,
                                                  &mark->source0,
                                                  &mark->index0,
                                                  &mark->count0,
                                                  &mark->source1,
                                                  &mark->index1,
                                                  &mark->count1);
}

guint32
hyscan_waterfall_mark_data_get_mod_count (HyScanWaterfallMarkData *self)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (self), 0);

  return hyscan_db_get_mod_count (self->priv->db, self->priv->param_id);
}

/*
 * \file hyscan-waterfall-mark-data.c
 *
 * \brief Исходный файл класса работы с метками водопада
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-waterfall-mark-data.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define MARK_ID_LEN 20
#define GROUP_NAME "waterfall-marks"

enum
{
  PROP_0,
  PROP_DB,
  PROP_PROJECT,
};

struct _HyScanWaterfallMarkDataPrivate
{
  HyScanDB          *db;       /* Интерфейс БД. */
  gchar             *project;  /* Проект. */
  gint32             param_id; /* Идентификатор группы параметров. */

  GRand             *rand;     /* Генератор случайных чисел. */
};

static void    hyscan_waterfall_mark_data_set_property            (GObject                         *object,
                                                                   guint                            prop_id,
                                                                   const GValue                    *value,
                                                                   GParamSpec                      *pspec);
static void    hyscan_waterfall_mark_data_object_constructed      (GObject                         *object);
static void    hyscan_waterfall_mark_data_object_finalize         (GObject                         *object);

static gchar  *hyscan_waterfall_mark_data_generate_id             (GRand                           *rand);

static gboolean hyscan_waterfall_mark_data_get_internal           (HyScanWaterfallMarkDataPrivate  *priv,
                                                                   const gchar                     *id,
                                                                   HyScanWaterfallMark             *mark);
static gboolean hyscan_waterfall_mark_data_set_internal           (HyScanWaterfallMarkDataPrivate  *priv,
                                                                   const gchar                     *id,
                                                                   const HyScanWaterfallMark       *mark);

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

}

static void
hyscan_waterfall_mark_data_init (HyScanWaterfallMarkData *data)
{
  data->priv = hyscan_waterfall_mark_data_get_instance_private (data);
}

static void
hyscan_waterfall_mark_data_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanWaterfallMarkData *data = HYSCAN_WATERFALL_MARK_DATA (object);
  HyScanWaterfallMarkDataPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
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

  HyScanWaterfallMarkData *data = HYSCAN_WATERFALL_MARK_DATA (object);
  HyScanWaterfallMarkDataPrivate *priv = data->priv;

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

  /* Открываем (или создаем) группу параметров. */
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, GROUP_NAME);
  if (priv->param_id <= 0)
    {
      g_warning ("HyScanWaterfallMarkData: can't open group %s (project '%s')",
                 GROUP_NAME, priv->project);
      goto exit;
    }

  priv->rand = g_rand_new ();

exit:
  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
}

static void
hyscan_waterfall_mark_data_object_finalize (GObject *object)
{
  HyScanWaterfallMarkData *data = HYSCAN_WATERFALL_MARK_DATA (object);
  HyScanWaterfallMarkDataPrivate *priv = data->priv;

  if (priv->rand != NULL)
    g_rand_free (priv->rand);

  g_free (priv->project);

  if (priv->param_id > 0)
    hyscan_db_close (priv->db, priv->param_id);

  g_object_unref (priv->db);

  G_OBJECT_CLASS (hyscan_waterfall_mark_data_parent_class)->finalize (object);
}

/* Функция генерирует идентификатор. */
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
                                         const gchar                    *id,
                                         HyScanWaterfallMark            *mark)
{
  const gchar *param_names[15];
  GVariant *param_values[15];
  gboolean status = FALSE;
  gint i;

  param_names[ 0] = "/schema/id";
  param_names[ 1] = "/schema/version";
  param_names[ 2] = "/track";
  param_names[ 3] = "/name";
  param_names[ 4] = "/description";
  param_names[ 5] = "/label";
  param_names[ 6] = "/operator";
  param_names[ 7] = "/time/creation";
  param_names[ 8] = "/time/modification";
  param_names[ 9] = "/coordinates/source0";
  param_names[10] = "/coordinates/index0";
  param_names[11] = "/coordinates/count0";
  param_names[12] = "/coordinates/width";
  param_names[13] = "/coordinates/height";
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

  if (mark == NULL)
    goto exit;

  hyscan_waterfall_mark_set_track  (mark, g_variant_get_string (param_values[2], NULL));
  hyscan_waterfall_mark_set_text   (mark, g_variant_get_string (param_values[3], NULL),
                                          g_variant_get_string (param_values[4], NULL),
                                          g_variant_get_string (param_values[6], NULL));
  hyscan_waterfall_mark_set_labels (mark, g_variant_get_int64 (param_values[5]));
  hyscan_waterfall_mark_set_ctime (mark, g_variant_get_int64 (param_values[7]));
  hyscan_waterfall_mark_set_mtime (mark, g_variant_get_int64 (param_values[8]));
  hyscan_waterfall_mark_set_center (mark, g_variant_get_int64 (param_values[9]),
                                          g_variant_get_int64 (param_values[10]),
                                          g_variant_get_int64 (param_values[11]));
  hyscan_waterfall_mark_set_size   (mark, g_variant_get_int64 (param_values[12]),
                                          g_variant_get_int64 (param_values[13]));

exit:
  for (i = 0; i < 14; i++)
    g_variant_unref (param_values[i]);

  return status;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_waterfall_mark_data_set_internal (HyScanWaterfallMarkDataPrivate *priv,
                                         const gchar                    *id,
                                         const HyScanWaterfallMark      *mark)
{
  const gchar *param_names[13];
  GVariant *param_values[13];
  gint i;

  param_names[ 0] = "/track";
  param_names[ 1] = "/name";
  param_names[ 2] = "/description";
  param_names[ 3] = "/label";
  param_names[ 4] = "/operator";
  param_names[ 5] = "/time/creation";
  param_names[ 6] = "/time/modification";
  param_names[ 7] = "/coordinates/source0";
  param_names[ 8] = "/coordinates/index0";
  param_names[ 9] = "/coordinates/count0";
  param_names[10] = "/coordinates/width";
  param_names[11] = "/coordinates/height";
  param_names[12] = NULL;

  param_values[ 0] = g_variant_new_string (mark->track);
  param_values[ 1] = g_variant_new_string (mark->name);
  param_values[ 2] = g_variant_new_string (mark->description);
  param_values[ 3] = g_variant_new_int64  (mark->labels);
  param_values[ 4] = g_variant_new_string (mark->operator_name);
  param_values[ 5] = g_variant_new_int64  (mark->creation_time);
  param_values[ 6] = g_variant_new_int64  (mark->modification_time);
  param_values[ 7] = g_variant_new_int64  (mark->source0);
  param_values[ 8] = g_variant_new_int64  (mark->index0);
  param_values[ 9] = g_variant_new_int64  (mark->count0);
  param_values[10] = g_variant_new_int64  (mark->width);
  param_values[11] = g_variant_new_int64  (mark->height);

  if (hyscan_db_param_set (priv->db, priv->param_id, id, param_names, param_values))
    return TRUE;

  for (i = 0; i < 12; i++)
    g_variant_unref (param_values[i]);

  return FALSE;
}

/* Функция создает новый объект работы с метками. */
HyScanWaterfallMarkData*
hyscan_waterfall_mark_data_new (HyScanDB    *db,
                                const gchar *project)
{
  HyScanWaterfallMarkData* mdata;
  mdata = g_object_new (HYSCAN_TYPE_WATERFALL_MARK_DATA,
                        "db", db,
                        "project", project,
                        NULL);

  if (mdata->priv->param_id <= 0)
    g_clear_object (&mdata);

  return mdata;
}

/* Функция добавляет метку в базу данных. */
gboolean
hyscan_waterfall_mark_data_add (HyScanWaterfallMarkData *data,
                                HyScanWaterfallMark     *mark)
{
  gchar *id;
  gboolean status;
  HyScanWaterfallMarkDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (data), FALSE);
  priv = data->priv;

  id = hyscan_waterfall_mark_data_generate_id (priv->rand);

  status = hyscan_db_param_object_create (priv->db, priv->param_id,
                                          id, WATERFALL_MARK_SCHEMA);
  if (!status)
    {
      g_info ("Failed to create object %s", id);
      goto exit;
    }

  status = hyscan_waterfall_mark_data_set_internal (priv, id, mark);

exit:
  g_free (id);
  return status;
}

/* Функция удаляет метку из базы данных. */
gboolean
hyscan_waterfall_mark_data_remove (HyScanWaterfallMarkData *data,
                                   const gchar             *id)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (data), FALSE);

  return hyscan_db_param_object_remove (data->priv->db,
                                        data->priv->param_id,
                                        id);
}

/* Функция изменяет метку. */
gboolean
hyscan_waterfall_mark_data_modify (HyScanWaterfallMarkData *data,
                                   const gchar             *id,
                                   HyScanWaterfallMark     *mark)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (data), FALSE);

  /* Проверяем, что метка существует. */
  if (!hyscan_waterfall_mark_data_get_internal (data->priv, id, NULL))
    return FALSE;

  return hyscan_waterfall_mark_data_set_internal (data->priv, id, mark);
}

/* Функция возвращает список идентификаторов всех меток. */
gchar**
hyscan_waterfall_mark_data_get_ids (HyScanWaterfallMarkData *data,
                                    guint                   *len)
{
  gchar** objects;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (data), NULL);

  objects = hyscan_db_param_object_list (data->priv->db, data->priv->param_id);

  if (len != NULL)
    *len = (objects != NULL) ? g_strv_length (objects) : 0;

  return objects;
}

/* Функция возвращает метку по идентификатору. */
HyScanWaterfallMark*
hyscan_waterfall_mark_data_get (HyScanWaterfallMarkData *data,
                                const gchar             *id)
{
  gboolean status;
  HyScanWaterfallMark *mark;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (data), FALSE);

  mark = hyscan_waterfall_mark_new ();

  status = hyscan_waterfall_mark_data_get_internal (data->priv, id, mark);

  if (!status)
    g_clear_pointer (&mark, hyscan_waterfall_mark_free);

  return mark;
}

/* Функция возвращает счётчик изменений. */
guint32
hyscan_waterfall_mark_data_get_mod_count (HyScanWaterfallMarkData *data)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_MARK_DATA (data), 0);

  return hyscan_db_get_mod_count (data->priv->db, data->priv->param_id);
}

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

  HyScanParamList   *read_plist;
  HyScanParamList   *write_plist;
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
  gint i;
  gint32 project_id = 0;      /* Идентификатор проекта. */
  const gchar *param_names[] = {"/track",
                                "/name",
                                "/description",
                                "/label",
                                "/operator",
                                "/time/creation",
                                "/time/modification",
                                "/coordinates/source0",
                                "/coordinates/index0",
                                "/coordinates/count0",
                                "/coordinates/width",
                                "/coordinates/height",
                                NULL};

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
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, WATERFALL_MARK_SCHEMA);
  if (priv->param_id <= 0)
    {
      g_warning ("HyScanWaterfallMarkData: can't open group %s (project '%s')",
                 WATERFALL_MARK_SCHEMA, priv->project);
      goto exit;
    }

  priv->read_plist = hyscan_param_list_new ();
  priv->write_plist = hyscan_param_list_new ();

  /* Добавляем названия параметров в списки. */
  for (i = 0; param_names[i] != NULL; ++i)
    {
      hyscan_param_list_add (priv->read_plist, param_names[i]);
      hyscan_param_list_add (priv->write_plist, param_names[i]);
    }

  /* В список на чтение дополнительно запихиваем version и id. */
  hyscan_param_list_add (priv->read_plist, "/schema/id");
  hyscan_param_list_add (priv->read_plist, "/schema/version");

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

  g_object_unref (priv->read_plist);
  g_object_unref (priv->write_plist);

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
  gint64 sid, sver;

  if (!hyscan_db_param_get (priv->db, priv->param_id, id, priv->read_plist))
    return FALSE;

  sid = hyscan_param_list_get_integer (priv->read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (priv->read_plist, "/schema/version");

  if (sid != WATERFALL_MARK_SCHEMA_ID || sver != WATERFALL_MARK_SCHEMA_VERSION)
    return FALSE;

  if (mark != NULL)
    {
      hyscan_waterfall_mark_set_track  (mark,
                                        hyscan_param_list_get_string (priv->read_plist,"/track"));
      hyscan_waterfall_mark_set_text   (mark,
                                        hyscan_param_list_get_string (priv->read_plist,"/name"),
                                        hyscan_param_list_get_string (priv->read_plist,"/description"),
                                        hyscan_param_list_get_string (priv->read_plist,"/operator"));
      hyscan_waterfall_mark_set_labels (mark,
                                        hyscan_param_list_get_integer (priv->read_plist,"/label"));
      hyscan_waterfall_mark_set_ctime  (mark,
                                        hyscan_param_list_get_integer (priv->read_plist,"/time/creation"));
      hyscan_waterfall_mark_set_mtime  (mark,
                                        hyscan_param_list_get_integer (priv->read_plist,"/time/modification"));
      hyscan_waterfall_mark_set_center (mark,
                                        hyscan_param_list_get_integer (priv->read_plist,"/coordinates/source0"),
                                        hyscan_param_list_get_integer (priv->read_plist,"/coordinates/index0"),
                                        hyscan_param_list_get_integer (priv->read_plist,"/coordinates/count0"));
      hyscan_waterfall_mark_set_size   (mark,
                                        hyscan_param_list_get_integer (priv->read_plist,"/coordinates/width"),
                                        hyscan_param_list_get_integer (priv->read_plist,"/coordinates/height"));
    }

  return TRUE;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_waterfall_mark_data_set_internal (HyScanWaterfallMarkDataPrivate *priv,
                                         const gchar                    *id,
                                         const HyScanWaterfallMark      *mark)
{
  hyscan_param_list_set (priv->write_plist, "/track",
                         g_variant_new_string (mark->track));
  hyscan_param_list_set (priv->write_plist, "/name",
                         g_variant_new_string (mark->name));
  hyscan_param_list_set (priv->write_plist, "/description",
                         g_variant_new_string (mark->description));
  hyscan_param_list_set (priv->write_plist, "/label",
                         g_variant_new_int64  (mark->labels));
  hyscan_param_list_set (priv->write_plist, "/operator",
                         g_variant_new_string (mark->operator_name));
  hyscan_param_list_set (priv->write_plist, "/time/creation",
                         g_variant_new_int64  (mark->creation_time));
  hyscan_param_list_set (priv->write_plist, "/time/modification",
                         g_variant_new_int64  (mark->modification_time));
  hyscan_param_list_set (priv->write_plist, "/coordinates/source0",
                         g_variant_new_int64  (mark->source0));
  hyscan_param_list_set (priv->write_plist, "/coordinates/index0",
                         g_variant_new_int64  (mark->index0));
  hyscan_param_list_set (priv->write_plist, "/coordinates/count0",
                         g_variant_new_int64  (mark->count0));
  hyscan_param_list_set (priv->write_plist, "/coordinates/width",
                         g_variant_new_int64  (mark->width));
  hyscan_param_list_set (priv->write_plist, "/coordinates/height",
                         g_variant_new_int64  (mark->height));

  return hyscan_db_param_set (priv->db, priv->param_id, id, priv->write_plist);
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

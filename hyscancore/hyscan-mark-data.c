/*
 * \file hyscan-mark-data.c
 *
 * \brief Исходный файл класса работы с метками
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-mark-data.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define MARK_ID_LEN 20

enum
{
  PROP_0,
  PROP_DB,
  PROP_PROJECT,
};

struct _HyScanMarkDataPrivate
{
  HyScanDB          *db;       /* Интерфейс БД. */
  gchar             *project;  /* Проект. */
  gint32             param_id; /* Идентификатор группы параметров. */

  HyScanParamList   *read_plist;
  HyScanParamList   *write_plist;
};

static void     hyscan_mark_data_set_property            (GObject                *object,
                                                          guint                   prop_id,
                                                          const GValue           *value,
                                                          GParamSpec             *pspec);
static void     hyscan_mark_data_object_constructed      (GObject                *object);
static void     hyscan_mark_data_object_finalize         (GObject                *object);

static gchar *  hyscan_mark_data_generate_id             (void);

static gboolean hyscan_mark_data_get_internal            (HyScanMarkData         *data,
                                                          const gchar            *id,
                                                          HyScanMark             *mark);
static gboolean hyscan_mark_data_set_internal            (HyScanMarkData         *data,
                                                          const gchar            *id,
                                                          const HyScanMark       *mark);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkData, hyscan_mark_data, G_TYPE_OBJECT);

static void
hyscan_mark_data_class_init (HyScanMarkDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_data_set_property;

  object_class->constructed = hyscan_mark_data_object_constructed;
  object_class->finalize = hyscan_mark_data_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "Project name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_mark_data_init (HyScanMarkData *data)
{
  data->priv = hyscan_mark_data_get_instance_private (data);
}

static void
hyscan_mark_data_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanMarkData *data = HYSCAN_MARK_DATA (object);
  HyScanMarkDataPrivate *priv = data->priv;

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
hyscan_mark_data_object_constructed (GObject *object)
{
  gint i;
  gint32 project_id = 0;      /* Идентификатор проекта. */
  const gchar *param_names[] = {"/name",
                                "/description",
                                "/label",
                                "/operator",
                                "/time/creation",
                                "/time/modification",
                                "/coordinates/width",
                                "/coordinates/height",
                                NULL};

  HyScanMarkData *data = HYSCAN_MARK_DATA (object);
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);
  HyScanMarkDataPrivate *priv = data->priv;

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

  /* Добавляем названия параметров из наследников. */
  if (klass->init_plist != NULL)
    {
      klass->init_plist (data, priv->read_plist);
      klass->init_plist (data, priv->write_plist);
    }

  /* В список на чтение дополнительно запихиваем version и id. */
  hyscan_param_list_add (priv->read_plist, "/schema/id");
  hyscan_param_list_add (priv->read_plist, "/schema/version");

exit:
  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
}

static void
hyscan_mark_data_object_finalize (GObject *object)
{
  HyScanMarkData *data = HYSCAN_MARK_DATA (object);
  HyScanMarkDataPrivate *priv = data->priv;

  g_free (priv->project);

  if (priv->param_id > 0)
    hyscan_db_close (priv->db, priv->param_id);

  g_clear_object (&priv->db);

  G_OBJECT_CLASS (hyscan_mark_data_parent_class)->finalize (object);
}

/* Функция генерирует идентификатор. */
static gchar*
hyscan_mark_data_generate_id (void)
{
  static gchar dict[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  guint i;
  gchar *id;

  id = g_malloc (MARK_ID_LEN + 1);
  id[MARK_ID_LEN] = '\0';

  for (i = 0; i < MARK_ID_LEN; i++)
    {
      gint rnd = g_random_int_range (0, sizeof dict - 1);
      id[i] = dict[rnd];
    }

  return id;
}

/* Функция возвращает информация о схеме меток. */
static const gchar *
hyscan_mark_data_get_schema_info (HyScanMarkData *data,
                                  gint64         *id,
                                  gint64         *version)
{
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);

  g_return_val_if_fail (klass->get_schema != NULL, NULL);

  return klass->get_schema (data, id, version);
}

/* Функция считывает содержимое объекта. */
static gboolean
hyscan_mark_data_get_internal (HyScanMarkData *data,
                               const gchar    *id,
                               HyScanMark     *mark)
{
  HyScanMarkDataPrivate *priv = data->priv;
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);
  gint64 sid, sver;
  gint64 data_sid, data_sver;

  if (!hyscan_db_param_get (priv->db, priv->param_id, id, priv->read_plist))
    return FALSE;

  hyscan_mark_data_get_schema_info (data, &data_sid, &data_sver);
  sid = hyscan_param_list_get_integer (priv->read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (priv->read_plist, "/schema/version");

  if (sid != data_sid || sver != data_sver)
    return FALSE;

  if (mark != NULL)
    {
      hyscan_mark_set_text   (mark,
                              hyscan_param_list_get_string (priv->read_plist,  "/name"),
                              hyscan_param_list_get_string (priv->read_plist,  "/description"),
                              hyscan_param_list_get_string (priv->read_plist,  "/operator"));
      hyscan_mark_set_labels (mark,
                              hyscan_param_list_get_integer (priv->read_plist, "/label"));
      hyscan_mark_set_ctime  (mark,
                              hyscan_param_list_get_integer (priv->read_plist, "/time/creation"));
      hyscan_mark_set_mtime  (mark,
                              hyscan_param_list_get_integer (priv->read_plist, "/time/modification"));
      hyscan_mark_set_size   (mark,
                              hyscan_param_list_get_integer (priv->read_plist, "/coordinates/width"),
                              hyscan_param_list_get_integer (priv->read_plist, "/coordinates/height"));

      if (klass->get != NULL)
        klass->get (data, priv->read_plist, mark);
    }

  return TRUE;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_mark_data_set_internal (HyScanMarkData    *data,
                               const gchar       *id,
                               const HyScanMark  *mark)
{
  HyScanMarkDataPrivate *priv = data->priv;
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  hyscan_param_list_set (priv->write_plist, "/name",
                         g_variant_new_string (any->name));
  hyscan_param_list_set (priv->write_plist, "/description",
                         g_variant_new_string (any->description));
  hyscan_param_list_set (priv->write_plist, "/label",
                         g_variant_new_int64  (any->labels));
  hyscan_param_list_set (priv->write_plist, "/operator",
                         g_variant_new_string (any->operator_name));
  hyscan_param_list_set (priv->write_plist, "/time/creation",
                         g_variant_new_int64  (any->creation_time));
  hyscan_param_list_set (priv->write_plist, "/time/modification",
                         g_variant_new_int64  (any->modification_time));
  hyscan_param_list_set (priv->write_plist, "/coordinates/width",
                         g_variant_new_int64  (any->width));
  hyscan_param_list_set (priv->write_plist, "/coordinates/height",
                         g_variant_new_int64  (any->height));

  if (klass->set != NULL)
    klass->set (data, priv->write_plist, mark);

  return hyscan_db_param_set (priv->db, priv->param_id, id, priv->write_plist);
}

/* Функция добавляет метку в базу данных. */
gboolean
hyscan_mark_data_add (HyScanMarkData *data,
                      HyScanMark     *mark)
{
  gchar *id;
  gboolean status;
  HyScanMarkDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);
  priv = data->priv;

  id = hyscan_mark_data_generate_id ();

  status = hyscan_db_param_object_create (priv->db, priv->param_id,
                                          id, WATERFALL_MARK_SCHEMA);
  if (!status)
    {
      g_info ("Failed to create object %s", id);
      goto exit;
    }

  status = hyscan_mark_data_set_internal (data, id, mark);

exit:
  g_free (id);
  return status;
}

/* Функция удаляет метку из базы данных. */
gboolean
hyscan_mark_data_remove (HyScanMarkData *data,
                         const gchar    *id)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);

  return hyscan_db_param_object_remove (data->priv->db,
                                        data->priv->param_id,
                                        id);
}

/* Функция изменяет метку. */
gboolean
hyscan_mark_data_modify (HyScanMarkData *data,
                         const gchar    *id,
                         HyScanMark     *mark)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);

  /* Проверяем, что метка существует. */
  if (!hyscan_mark_data_get_internal (data, id, NULL))
    return FALSE;

  return hyscan_mark_data_set_internal (data, id, mark);
}

/* Функция возвращает список идентификаторов всех меток. */
gchar**
hyscan_mark_data_get_ids (HyScanMarkData *data,
                          guint          *len)
{
  gchar** objects;

  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), NULL);

  objects = hyscan_db_param_object_list (data->priv->db, data->priv->param_id);

  if (len != NULL)
    *len = (objects != NULL) ? g_strv_length (objects) : 0;

  return objects;
}

/* Функция возвращает метку по идентификатору. */
HyScanMark*
hyscan_mark_data_get (HyScanMarkData *data,
                      const gchar    *id)
{
  gboolean status;
  HyScanMark *mark = NULL;

  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);

  mark = hyscan_mark_new ();

  status = hyscan_mark_data_get_internal (data, id, mark);

  if (!status)
    g_clear_pointer (&mark, hyscan_mark_free);

  return mark;
}

/* Функция возвращает счётчик изменений. */
guint32
hyscan_mark_data_get_mod_count (HyScanMarkData *data)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), 0);

  return hyscan_db_get_mod_count (data->priv->db, data->priv->param_id);
}

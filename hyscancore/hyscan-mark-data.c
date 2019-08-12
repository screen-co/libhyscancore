/* hyscan-mark-data.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-mark-data
 * @Short_description: Абстрактный класс работы с метками
 * @Title: HyScanMarkData
 * @See_also: HyScanMarkDataWaterfall
 *
 * HyScanMarkData - абстрактный класс позволяющий работать с различными типами меток.
 * Он представляет собой обертку над базой данных, что позволяет потребителю работать
 * не с конкретными записями в базе данных, а с метками и их идентификаторами.
 *
 * Класс решает задачи добавления, удаления, модификации и получения меток.
 *
 * Класс не потокобезопасен.
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
static void     hyscan_mark_data_get_property            (GObject                *object,
                                                          guint                   prop_id,
                                                          GValue                 *value,
                                                          GParamSpec             *pspec);
static void     hyscan_mark_data_object_constructed      (GObject                *object);
static void     hyscan_mark_data_object_finalize         (GObject                *object);

static gchar *  hyscan_mark_data_generate_id             (HyScanMarkData         *data,
                                                          gpointer                mark);

static gboolean hyscan_mark_data_get_internal            (HyScanMarkData         *data,
                                                          const gchar            *id,
                                                          HyScanMark             *mark);
static gboolean hyscan_mark_data_set_internal            (HyScanMarkData         *data,
                                                          const gchar            *id,
                                                          const HyScanMark       *mark);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanMarkData, hyscan_mark_data, G_TYPE_OBJECT);

static void
hyscan_mark_data_class_init (HyScanMarkDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanMarkDataClass *data_class = HYSCAN_MARK_DATA_CLASS (klass);

  object_class->set_property = hyscan_mark_data_set_property;
  object_class->get_property = hyscan_mark_data_get_property;

  object_class->constructed = hyscan_mark_data_object_constructed;
  object_class->finalize = hyscan_mark_data_object_finalize;

  data_class->generate_id = hyscan_mark_data_generate_id;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "Project name", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

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
hyscan_mark_data_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HyScanMarkData *data = HYSCAN_MARK_DATA (object);
  HyScanMarkDataPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DB:
      g_value_set_object (value, priv->db);
      break;

    case PROP_PROJECT:
      g_value_set_string (value, priv->project);
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
                                "/operator",
                                "/label",
                                "/ctime",
                                "/mtime",
                                "/width",
                                "/height",
                                NULL};

  HyScanMarkData *data = HYSCAN_MARK_DATA (object);
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);
  HyScanMarkDataPrivate *priv = data->priv;

  if (priv->db == NULL)
    {
      g_warning ("HyScanMarkData: db not specified");
      goto exit;
    }

  /* Открываем проект. */
  project_id = hyscan_db_project_open (priv->db, priv->project);
  if (project_id <= 0)
    {
      g_warning ("HyScanMarkData: can't open project '%s'", priv->project);
      goto exit;
    }

  /* Открываем (или создаем) группу параметров. */
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, klass->group_name);
  if (priv->param_id <= 0)
    {
      g_warning ("HyScanMarkData: can't open group %s (project '%s')",
                 klass->group_name, priv->project);
      goto exit;
    }

  priv->read_plist = hyscan_param_list_new ();
  priv->write_plist = hyscan_param_list_new ();

  /* Добавляем названия параметров в списки. */
  for (i = 0; param_names[i] != NULL; ++i)
    hyscan_param_list_add (priv->read_plist, param_names[i]);

  /* В список на чтение дополнительно запихиваем version и id. */
  hyscan_param_list_add (priv->read_plist, "/schema/id");
  hyscan_param_list_add (priv->read_plist, "/schema/version");

  if (klass->init_obj != NULL)
    klass->init_obj (data, priv->param_id, priv->db);

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

  g_clear_object (&priv->read_plist);
  g_clear_object (&priv->write_plist);

  G_OBJECT_CLASS (hyscan_mark_data_parent_class)->finalize (object);
}

/* Функция генерирует идентификатор. */
static gchar*
hyscan_mark_data_generate_id (HyScanMarkData *data,
                              gpointer        mark)
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

/* Функция считывает содержимое объекта. */
static gboolean
hyscan_mark_data_get_internal (HyScanMarkData *data,
                               const gchar    *id,
                               HyScanMark     *mark)
{
  HyScanMarkDataPrivate *priv = data->priv;
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);

  HyScanDataSchema *schema = NULL;
  const gchar *schema_id;
  HyScanParamList *read_plist = NULL;
  gboolean read_status;

  g_return_val_if_fail (klass->get_full != NULL, FALSE);

  /* Определяем схему данных запрошенного объекта. */
  schema = hyscan_db_param_object_get_schema (priv->db, priv->param_id, id);
  if (schema == NULL)
    {
      g_warning ("HyScanMarkData: failed to determine schema of <%s>", id);
      return FALSE;
    }
  schema_id = hyscan_data_schema_get_id (schema);

  /* Получаем список параметров для чтения этой схемы. */
  if (klass->get_read_plist != NULL)
    read_plist = klass->get_read_plist (data, schema_id);
  else
    read_plist = g_object_ref (priv->read_plist);

  /* Считываем параметры объекта и формируем из него структуру. */
  read_status = hyscan_db_param_get (priv->db, priv->param_id, id, read_plist);
  if (!read_status)
    goto exit;

  read_status = klass->get_full (data, read_plist, mark);

exit:
  g_clear_object (&schema);
  g_clear_object (&read_plist);

  return read_status;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_mark_data_set_internal (HyScanMarkData    *data,
                               const gchar       *id,
                               const HyScanMark  *mark)
{
  HyScanMarkDataPrivate *priv = data->priv;
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);

  hyscan_param_list_clear (priv->write_plist);

  if (klass->set_full != NULL)
    klass->set_full (data, priv->write_plist, mark);

  return hyscan_db_param_set (priv->db, priv->param_id, id, priv->write_plist);
}

/**
 * hyscan_mark_data_is_ready:
 * @data: указатель на объект #HyScanMarkData
 *
 * Проверяет, корректно ли проинициализировался объект. Если нет, то он будет
 * неработоспособен
 *
 * Returns: %TRUE, если инициализация успешна.
 */

gboolean
hyscan_mark_data_is_ready (HyScanMarkData *data)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);

  return data->priv->param_id > 0;
}

/**
 * hyscan_mark_data_add:
 * @data: указатель на объект #HyScanMarkData
 * @mark: указатель на структуру #HyScanMark
 *
 * Функция добавляет метку в базу данных.
 *
 * Returns: %TRUE, если удалось добавить метку, иначе %FALSE.
 */
gboolean
hyscan_mark_data_add (HyScanMarkData  *data,
                      gpointer         mark,
                      gchar          **given_id)
{
  gchar *id;
  gboolean status = FALSE;
  HyScanMarkDataPrivate *priv;
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);
  const gchar *schema_id = NULL;

  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);
  priv = data->priv;

  id = klass->generate_id (data, mark);
  if (id == NULL)
    {
      g_message ("HyScanMarkData: failed to generate object id");
      goto exit;
    }

  if (klass->get_schema_id != NULL)
    schema_id = klass->get_schema_id (data, mark);

  if (schema_id == NULL)
    {
      g_message ("HyScanMarkData: undefined schema of object %s", id);
      goto exit;
    }

  status = hyscan_db_param_object_create (priv->db, priv->param_id, id, schema_id);
  if (!status)
    {
      g_info ("Failed to create object %s", id);
      goto exit;
    }

  status = hyscan_mark_data_set_internal (data, id, mark);

exit:
  if (status && given_id != NULL)
    *given_id = id;
  else
    g_free (id);

  return status;
}

/**
 * hyscan_mark_data_remove:
 * @data: указатель на объект #HyScanMarkData
 * @id: идентификатор метки
 *
 * Функция удаляет метку из базы данных.
 *
 * Returns: %TRUE, если удалось удалить метку, иначе %FALSE.
 */
gboolean
hyscan_mark_data_remove (HyScanMarkData *data,
                         const gchar    *id)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);

  return hyscan_db_param_object_remove (data->priv->db,
                                        data->priv->param_id,
                                        id);
}

/**
 * hyscan_mark_data_modify:
 * @data: указатель на объект #HyScanMarkData
 * @id: идентификатор метки
 * @mark: указатель на структуру #HyScanMark
 *
 * Функция изменяет метку.
 *
 * Returns: %TRUE, если удалось изменить метку, иначе %FALSE
 */
gboolean
hyscan_mark_data_modify (HyScanMarkData *data,
                         const gchar    *id,
                         gconstpointer   mark)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);

  /* Проверяем, что метка существует. */
  if (!hyscan_mark_data_get_internal (data, id, NULL))
    return FALSE;

  return hyscan_mark_data_set_internal (data, id, mark);
}

/**
 * hyscan_mark_data_get_ids:
 * @data: указатель на объект #HyScanMarkData
 * @len: количество элементов или %NULL
 *
 * Функция возвращает список идентификаторов всех меток.
 *
 * Returns: (array length=len): (transfer full): %NULL-терминированный список
 *   идентификаторов, %NULL если меток нет. Для удаления g_strfreev()
 */
gchar **
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

/**
 * hyscan_mark_data_get:
 * @data: указатель на объект #HyScanMarkData
 * @id: идентификатор метки
 *
 * Функция возвращает метку по идентификатору.
 *
 * Returns: указатель на структуру #HyScanMark, %NULL в случае ошибки. Для
 *   удаления hyscan_mark_free().
 */
gpointer
hyscan_mark_data_get (HyScanMarkData *data,
                      const gchar    *id)
{
  HyScanMarkDataClass *klass;
  gboolean status;
  HyScanMark *mark = NULL;

  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), FALSE);
  klass = HYSCAN_MARK_DATA_GET_CLASS (data);

  mark = klass->object_new (data, id);

  status = hyscan_mark_data_get_internal (data, id, mark);

  if (!status)
    g_clear_pointer (&mark, klass->object_destroy);

  return mark;
}

/**
 * hyscan_mark_data_get_mod_count:
 *
 * @data: указатель на объект #HyScanMarkData
 *
 * Функция возвращает счётчик изменений.
 *
 * Returns: номер изменения.
 */
guint32
hyscan_mark_data_get_mod_count (HyScanMarkData *data)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), 0);

  return hyscan_db_get_mod_count (data->priv->db, data->priv->param_id);
}

gpointer
hyscan_mark_data_copy (HyScanMarkData *data,
                       gconstpointer   mark)
{
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);

  g_return_val_if_fail (HYSCAN_IS_MARK_DATA (data), NULL);

  return klass->object_copy (mark);
}

void
hyscan_mark_data_destroy (HyScanMarkData *data,
                          gpointer        mark)
{
  HyScanMarkDataClass *klass = HYSCAN_MARK_DATA_GET_CLASS (data);

  g_return_if_fail (HYSCAN_IS_MARK_DATA (data));

  klass->object_destroy (mark);
}

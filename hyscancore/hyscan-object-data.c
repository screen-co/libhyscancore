/* hyscan-object-data.c
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
 * SECTION: hyscan-object-data
 * @Short_description: Абстрактный класс работы с метками
 * @Title: HyScanObjectData
 * @See_also: HyScanObjectDataWaterfall
 *
 * HyScanObjectData - абстрактный класс позволяющий работать с объектами из
 * параметров проекта базы данных. Он представляет собой обертку над базой данных,
 * что позволяет потребителю работать не с конкретными записями в базе данных,
 * а с структурами C и их идентификаторами.
 *
 * Класс решает задачи добавления, удаления, модификации и получения объектов.
 *
 * Класс не потокобезопасен.
 */

#include "hyscan-object-data.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define OBJECT_ID_LEN 20

enum
{
  PROP_0,
  PROP_DB,
  PROP_PROJECT,
};

struct _HyScanObjectDataPrivate
{
  HyScanDB          *db;       /* Интерфейс БД. */
  gchar             *project;  /* Проект. */
  gint32             param_id; /* Идентификатор группы параметров. */

  HyScanParamList   *read_plist;
  HyScanParamList   *write_plist;
};

static void     hyscan_object_data_set_property            (GObject                *object,
                                                          guint                   prop_id,
                                                          const GValue           *value,
                                                          GParamSpec             *pspec);
static void     hyscan_object_data_get_property            (GObject                *object,
                                                          guint                   prop_id,
                                                          GValue                 *value,
                                                          GParamSpec             *pspec);
static void     hyscan_object_data_object_constructed      (GObject                *object);
static void     hyscan_object_data_object_finalize         (GObject                *object);

static gchar *  hyscan_object_data_generate_id             (HyScanObjectData         *data,
                                                            const HyScanObject       *object);

static gboolean hyscan_object_data_get_internal            (HyScanObjectData         *data,
                                                          const gchar            *id,
                                                          HyScanObject             *object);
static gboolean hyscan_object_data_set_internal            (HyScanObjectData         *data,
                                                          const gchar            *id,
                                                          const HyScanObject       *object);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanObjectData, hyscan_object_data, G_TYPE_OBJECT);

static void
hyscan_object_data_class_init (HyScanObjectDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class = HYSCAN_OBJECT_DATA_CLASS (klass);

  object_class->set_property = hyscan_object_data_set_property;
  object_class->get_property = hyscan_object_data_get_property;

  object_class->constructed = hyscan_object_data_object_constructed;
  object_class->finalize = hyscan_object_data_object_finalize;

  data_class->generate_id = hyscan_object_data_generate_id;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "Project name", NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_object_data_init (HyScanObjectData *data)
{
  data->priv = hyscan_object_data_get_instance_private (data);
}

static void
hyscan_object_data_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (object);
  HyScanObjectDataPrivate *priv = data->priv;

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
hyscan_object_data_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (object);
  HyScanObjectDataPrivate *priv = data->priv;

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
hyscan_object_data_object_constructed (GObject *object)
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

  HyScanObjectData *data = HYSCAN_OBJECT_DATA (object);
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  HyScanObjectDataPrivate *priv = data->priv;

  if (priv->db == NULL)
    {
      g_warning ("HyScanObjectData: db not specified");
      goto exit;
    }

  /* Открываем проект. */
  project_id = hyscan_db_project_open (priv->db, priv->project);
  if (project_id <= 0)
    {
      g_warning ("HyScanObjectData: can't open project '%s'", priv->project);
      goto exit;
    }

  /* Открываем (или создаем) группу параметров. */
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, klass->group_name);
  if (priv->param_id <= 0)
    {
      g_warning ("HyScanObjectData: can't open group %s (project '%s')",
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
hyscan_object_data_object_finalize (GObject *object)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (object);
  HyScanObjectDataPrivate *priv = data->priv;

  g_free (priv->project);

  if (priv->param_id > 0)
    hyscan_db_close (priv->db, priv->param_id);

  g_clear_object (&priv->db);

  g_clear_object (&priv->read_plist);
  g_clear_object (&priv->write_plist);

  G_OBJECT_CLASS (hyscan_object_data_parent_class)->finalize (object);
}

/* Функция генерирует идентификатор. */
static gchar*
hyscan_object_data_generate_id (HyScanObjectData   *data,
                                const HyScanObject *object)
{
  static gchar dict[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  guint i;
  gchar *id;

  id = g_malloc (OBJECT_ID_LEN + 1);
  id[OBJECT_ID_LEN] = '\0';

  for (i = 0; i < OBJECT_ID_LEN; i++)
    {
      gint rnd = g_random_int_range (0, sizeof dict - 1);
      id[i] = dict[rnd];
    }

  return id;
}

/* Функция считывает содержимое объекта. */
static gboolean
hyscan_object_data_get_internal (HyScanObjectData *data,
                               const gchar    *id,
                               HyScanObject     *object)
{
  HyScanObjectDataPrivate *priv = data->priv;
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  HyScanDataSchema *schema = NULL;
  const gchar *schema_id;
  HyScanParamList *read_plist = NULL;
  gboolean read_status;

  g_return_val_if_fail (klass->get_full != NULL, FALSE);

  /* Определяем схему данных запрошенного объекта. */
  schema = hyscan_db_param_object_get_schema (priv->db, priv->param_id, id);
  if (schema == NULL)
    {
      g_warning ("HyScanObjectData: failed to determine schema of <%s>", id);
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

  read_status = klass->get_full (data, read_plist, object);

exit:
  g_clear_object (&schema);
  g_clear_object (&read_plist);

  return read_status;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_object_data_set_internal (HyScanObjectData    *data,
                               const gchar       *id,
                               const HyScanObject  *object)
{
  HyScanObjectDataPrivate *priv = data->priv;
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  hyscan_param_list_clear (priv->write_plist);

  if (klass->set_full != NULL)
    klass->set_full (data, priv->write_plist, object);

  return hyscan_db_param_set (priv->db, priv->param_id, id, priv->write_plist);
}

/**
 * hyscan_object_data_is_ready:
 * @data: указатель на объект #HyScanObjectData
 *
 * Проверяет, корректно ли проинициализировался объект. Если нет, то он будет
 * неработоспособен
 *
 * Returns: %TRUE, если инициализация успешна.
 */

gboolean
hyscan_object_data_is_ready (HyScanObjectData *data)
{
  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);

  return data->priv->param_id > 0;
}

/**
 * hyscan_object_data_add:
 * @data: указатель на объект #HyScanObjectData
 * @object: указатель на структуру #HyScanObject
 *
 * Функция добавляет метку в базу данных.
 *
 * Returns: %TRUE, если удалось добавить метку, иначе %FALSE.
 */
gboolean
hyscan_object_data_add (HyScanObjectData  *data,
                        HyScanObject      *object,
                        gchar            **given_id)
{
  gchar *id;
  gboolean status = FALSE;
  HyScanObjectDataPrivate *priv;
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  const gchar *schema_id = NULL;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);
  priv = data->priv;

  id = klass->generate_id (data, object);
  if (id == NULL)
    {
      g_message ("HyScanObjectData: failed to generate object id");
      goto exit;
    }

  if (klass->get_schema_id != NULL)
    schema_id = klass->get_schema_id (data, object);

  if (schema_id == NULL)
    {
      g_message ("HyScanObjectData: undefined schema of object %s", id);
      goto exit;
    }

  status = hyscan_db_param_object_create (priv->db, priv->param_id, id, schema_id);
  if (!status)
    {
      g_info ("Failed to create object %s", id);
      goto exit;
    }

  status = hyscan_object_data_set_internal (data, id, object);

exit:
  if (status && given_id != NULL)
    *given_id = id;
  else
    g_free (id);

  return status;
}

/**
 * hyscan_object_data_remove:
 * @data: указатель на объект #HyScanObjectData
 * @id: идентификатор метки
 *
 * Функция удаляет метку из базы данных.
 *
 * Returns: %TRUE, если удалось удалить метку, иначе %FALSE.
 */
gboolean
hyscan_object_data_remove (HyScanObjectData *data,
                         const gchar    *id)
{
  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);

  return hyscan_db_param_object_remove (data->priv->db,
                                        data->priv->param_id,
                                        id);
}

/**
 * hyscan_object_data_modify:
 * @data: указатель на объект #HyScanObjectData
 * @id: идентификатор метки
 * @object: указатель на структуру #HyScanObject
 *
 * Функция изменяет метку.
 *
 * Returns: %TRUE, если удалось изменить метку, иначе %FALSE
 */
gboolean
hyscan_object_data_modify (HyScanObjectData   *data,
                           const gchar        *id,
                           const HyScanObject *object)
{
  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);

  /* Проверяем, что метка существует. */
  if (!hyscan_object_data_get_internal (data, id, NULL))
    return FALSE;

  return hyscan_object_data_set_internal (data, id, object);
}

/**
 * hyscan_object_data_get_ids:
 * @data: указатель на объект #HyScanObjectData
 * @len: количество элементов или %NULL
 *
 * Функция возвращает список идентификаторов всех меток.
 *
 * Returns: (array length=len): (transfer full): %NULL-терминированный список
 *   идентификаторов, %NULL если меток нет. Для удаления g_strfreev()
 */
gchar **
hyscan_object_data_get_ids (HyScanObjectData *data,
                          guint          *len)
{
  gchar** objects;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), NULL);

  objects = hyscan_db_param_object_list (data->priv->db, data->priv->param_id);

  if (len != NULL)
    *len = (objects != NULL) ? g_strv_length (objects) : 0;

  return objects;
}

/**
 * hyscan_object_data_get:
 * @data: указатель на объект #HyScanObjectData
 * @id: идентификатор метки
 *
 * Функция возвращает метку по идентификатору.
 *
 * Returns: указатель на структуру #HyScanObject, %NULL в случае ошибки. Для
 *   удаления hyscan_object_data_destroy().
 */
HyScanObject *
hyscan_object_data_get (HyScanObjectData *data,
                        const gchar      *id)
{
  HyScanObjectDataClass *klass;
  gboolean status;
  HyScanObject *object = NULL;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);
  klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  object = klass->object_new (data, id);

  status = hyscan_object_data_get_internal (data, id, object);

  if (!status)
    g_clear_pointer (&object, klass->object_destroy);

  return object;
}

/**
 * hyscan_object_data_get_mod_count:
 *
 * @data: указатель на объект #HyScanObjectData
 *
 * Функция возвращает счётчик изменений.
 *
 * Returns: номер изменения.
 */
guint32
hyscan_object_data_get_mod_count (HyScanObjectData *data)
{
  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), 0);

  return hyscan_db_get_mod_count (data->priv->db, data->priv->param_id);
}

HyScanObject *
hyscan_object_data_copy (HyScanObjectData   *data,
                         const HyScanObject *object)
{
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), NULL);

  return klass->object_copy (object);
}

void
hyscan_object_data_destroy (HyScanObjectData *data,
                            HyScanObject     *object)
{
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  g_return_if_fail (HYSCAN_IS_OBJECT_DATA (data));

  klass->object_destroy (object);
}

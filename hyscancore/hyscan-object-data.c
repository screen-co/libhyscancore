/* hyscan-object-data.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
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
 * SECTION: hyscan-object-data
 * @Short_description: Абстрактный класс работы с объектами параметров проекта
 * @Title: HyScanObjectData
 * @See_also: HyScanObjectDataWfmark, HyScanObjectDataGeomark
 *
 * HyScanObjectData - абстрактный класс, позволяющий работать с объектами из
 * параметров проекта базы данных. Он представляет собой обертку над базой данных,
 * что позволяет потребителю работать не с конкретными записями в базе данных,
 * а со структурами Си и их идентификаторами.
 *
 * HyScanObjectData работает только с одной группой параметров БД. При этом конкретная реализация класса
 * может хранить в этой группе объекты, имеющие различные схемы данных. Схема данных каждого объекта
 * определяется его идентификатором при помощи функции HyScanObjectDataClass.get_schema_id().
 *
 * Структуры Си, с которыми работает класс, должны быть зарегистрированы как
 * тип GBoxed и в качестве первого поля содержать GType-идентификатор этого типа. В этом
 * случае класс HyScanObjectData сможет определить тип структуры и корректно её обработать.
 * При передаче в функции класса все структуры приводятся к типу #HyScanObject.
 *
 * Конкретные реализации класса #HyScanObjectData находятся в классах:
 *
 * - #HyScanObjectDataWfmark - для работы со структурами #HyScanMarkWaterfall,
 * - #HyScanObjectDataGeomark - для работы со структурами #HyScanMarkGeo.
 *
 * Функции класса позволяют добавлять, удалять, модифицировать и получать объекты из БД.
 *
 * Класс не потокобезопасен.
 */

#include "hyscan-object-data.h"
#include <string.h>

#define OBJECT_ID_LEN 20       /* Длина идентификатора объекта. */

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
  HyScanParamList   *plist;    /* Список параметров для чтения объекта. */
};

static void           hyscan_object_data_interface_init        (HyScanObjectStoreInterface *iface);
static void           hyscan_object_data_get_property          (GObject                    *object,
                                                                guint                       prop_id,
                                                                GValue                     *value,
                                                                GParamSpec                 *pspec);
static void           hyscan_object_data_object_constructed    (GObject                    *object);
static void           hyscan_object_data_object_finalize       (GObject                    *object);
static gboolean       hyscan_object_data_add_real              (HyScanObjectData           *data,
                                                                const gchar                *id,
                                                                const HyScanObject         *object);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (HyScanObjectData, hyscan_object_data, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (HyScanObjectData)
                                  G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_OBJECT_STORE, hyscan_object_data_interface_init))

static void
hyscan_object_data_class_init (HyScanObjectDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = hyscan_object_data_get_property;

  object_class->constructed = hyscan_object_data_object_constructed;
  object_class->finalize = hyscan_object_data_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "Project name", NULL,
                           G_PARAM_READABLE));

}

static void
hyscan_object_data_init (HyScanObjectData *data)
{
  data->priv = hyscan_object_data_get_instance_private (data);
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
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (object);
  HyScanObjectDataPrivate *priv = data->priv;

  priv->plist = hyscan_param_list_new ();
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

  g_clear_object (&priv->plist);

  G_OBJECT_CLASS (hyscan_object_data_parent_class)->finalize (object);
}

/* Функция создания объекта в БД. */
static gboolean
hyscan_object_data_add_real (HyScanObjectData   *data,
                             const gchar        *id,
                             const HyScanObject *object)
{
  HyScanObjectDataPrivate *priv = data->priv;
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  const gchar *schema_id = NULL;

  if (klass->get_schema_id != NULL)
    schema_id = klass->get_schema_id (data, object);

  if (schema_id == NULL)
    {
      g_warning ("HyScanObjectData: undefined schema of object %s", id);
      return FALSE;
    }

  if (!hyscan_db_param_object_create (priv->db, priv->param_id, id, schema_id))
    {
      g_info ("Failed to create object %s", id);
      return FALSE;
    }

  return hyscan_object_store_modify (HYSCAN_OBJECT_STORE (data), id, object);
}

static gboolean
hyscan_object_data_add (HyScanObjectStore    *store,
                        const HyScanObject  *object,
                        gchar              **given_id)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);
  gboolean status;
  gchar *id;

  id = hyscan_object_data_generate_id (data, object);
  if (id == NULL)
    {
      g_warning ("HyScanObjectData: failed to generate object id");
      return FALSE;
    }

  status = hyscan_object_data_add_real (data, id, object);

  if (status && given_id != NULL)
    *given_id = id;
  else
    g_free (id);

  return status;
}

static gboolean
hyscan_object_data_remove (HyScanObjectStore *store,
                           GType              type,
                           const gchar       *id)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);
  const GType *types;
  guint i, len;
  gboolean accept_type = FALSE;

  /* Проверка типа. */
  types = hyscan_object_store_list_types (store, &len);
  for (i = 0; i < len && !accept_type; i++)
    accept_type = (types[i] == type);

  if (!accept_type)
    return FALSE;

  return hyscan_db_param_object_remove (data->priv->db,
                                        data->priv->param_id,
                                        id);
}

static gboolean
hyscan_object_data_modify (HyScanObjectStore  *store,
                           const gchar        *id,
                           const HyScanObject *object)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);
  HyScanObjectDataPrivate *priv = data->priv;
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  hyscan_param_list_clear (priv->plist);

  if (klass->set_full != NULL)
    klass->set_full (data, priv->plist, object);

  return hyscan_db_param_set (priv->db, priv->param_id, id, priv->plist);
}

/**
 * hyscan_object_data_set:
 * @data: указатель на объект #HyScanObjectData
 * @id: (allow-none): указатель на объект #HyScanObjectData
 * @object: (allow-none): указатель на структуру #HyScanObject
 *
 * Функция автоматически управляет объектами.
 * Если id задан, то объект создается или модифицируется (при отсутствии и
 * наличии в БД соответственно).
 * Если id не задан, объект создается. Если id задан, а object не задан, объект
 * удаляется.
 * Ошибкой является не задать и id, и object.
 *
 * Returns: %TRUE, если удалось добавить/удалить/модифицировать объект.
 */
gboolean
hyscan_object_data_set (HyScanObjectStore    *store,
                         GType                type,
                         const gchar         *id,
                         const HyScanObject  *object)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);
  HyScanObject *evil_twin;
  gboolean found;

  g_return_val_if_fail (id != NULL || (object != NULL && object->type == type), FALSE);

  /* id не задан -- просто создаем объект. */
  if (id == NULL)
    {
      return hyscan_object_store_add (store, object, NULL);
    }

  /* id задан -- ищем такой же объект в группе параметров. */
  evil_twin = hyscan_object_store_get (store, type, id);
  found = evil_twin != NULL;
  if (found && evil_twin->type != type)
    g_warning ("HyScanObjectData: setting object of another type");
  hyscan_object_free (evil_twin);

  /* Найден    и object == NULL -> удаление
   * Найден    и object != NULL -> правка
   * Не найден и object == NULL -> ничего
   * Не найден и object != NULL -> создание
   */
  if (found)
    {
      if (object == NULL)
        return hyscan_object_store_remove (store, type, id);
      else
        return hyscan_object_store_modify (store, id, object);
    }
  else
    {
      if (object != NULL)
        return hyscan_object_data_add_real (data, id, object);
      else
        return TRUE;
    }
}

GList *
hyscan_object_data_get_ids (HyScanObjectStore *store)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  gchar **objects;
  gint i;
  GList *list = NULL;

  objects = hyscan_db_param_object_list (data->priv->db, data->priv->param_id);

  if (objects == NULL)
    return NULL;

  for (i = 0; objects[i] != NULL; i++)
    {
      HyScanObjectId *id;

      id = hyscan_object_id_new ();
      id->type = klass->get_object_type (data, objects[i]);
      id->id = objects[i];

      list = g_list_append (list, id);
    }

  g_free (objects);

  return list;
}

static HyScanObject *
hyscan_object_data_get (HyScanObjectStore *store,
                        GType              type,
                        const gchar       *id)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);
  HyScanObjectDataPrivate *priv = data->priv;
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  HyScanParamList *read_plist = NULL;
  gboolean read_status;
  HyScanObject *object = NULL;

  g_return_val_if_fail (klass->get_full != NULL && klass->get_read_plist != NULL, NULL);

  /* Получаем список параметров для чтения объекта с этим идентификатором. */
  read_plist = klass->get_read_plist (data, id);

  /* Считываем параметры объекта и формируем из него структуру. */
  read_status = hyscan_db_param_get (priv->db, priv->param_id, id, read_plist);
  if (!read_status)
    goto exit;

  object = klass->get_full (data, read_plist);

  /* Проверяем тип объекта. */
  if (object != NULL && object->type != type)
    g_clear_pointer (&object, hyscan_object_free);

exit:
  g_clear_object (&read_plist);

  return object;
}

static guint32
hyscan_object_data_get_mod_count (HyScanObjectStore *store,
                                  GType              type)
{
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (store);

  /* Для всех типов объектов используем один и тот же счётчик изменений. */
  return hyscan_db_get_mod_count (data->priv->db, data->priv->param_id);
}

static GHashTable *
hyscan_object_data_get_all (HyScanObjectStore  *store,
                            GType               type)
{
  GHashTable *table;
  GList *ids, *link;

  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_object_free);
  ids = hyscan_object_store_get_ids (store);
  if (ids == NULL)
    return table;

  for (link = ids; link != NULL; link = link->next)
    {
      HyScanObjectId *id = link->data;
      HyScanObject *object;

      if (id->type != type)
        continue;

      object = hyscan_object_store_get (store, id->type, id->id);

      /* Возможно, объект уже исчез. */
      if (object == NULL)
        continue;

      g_hash_table_insert (table, g_strdup (id->id), object);
    }

  g_list_free_full (ids, (GDestroyNotify) hyscan_object_id_free);

  return table;
}

static const GType *
hyscan_object_data_list_types (HyScanObjectStore *store,
                               guint             *len)
{
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (store);

  len != NULL ? (*len = klass->n_data_types) : 0;

  return klass->data_types;
}

static void
hyscan_object_data_interface_init (HyScanObjectStoreInterface *iface)
{
  iface->get_mod_count = hyscan_object_data_get_mod_count;
  iface->get = hyscan_object_data_get;
  iface->modify = hyscan_object_data_modify;
  iface->get_ids = hyscan_object_data_get_ids;
  iface->add = hyscan_object_data_add;
  iface->get_all = hyscan_object_data_get_all;
  iface->remove = hyscan_object_data_remove;
  iface->set = hyscan_object_data_set;
  iface->list_types = hyscan_object_data_list_types;
}

/**
 * hyscan_object_data_new:
 * @type: тип класса, наследник #HyScanObjectData
 *
 * Создаёт новый объект для работы с параметрами. Для удаления g_object_unref().
 */
HyScanObjectData *
hyscan_object_data_new (GType type)
{
  g_return_val_if_fail (g_type_is_a (type, HYSCAN_TYPE_OBJECT_DATA), NULL);

  return g_object_new (type, NULL);
}

gboolean
hyscan_object_data_project_open (HyScanObjectData *data,
                                 HyScanDB         *db,
                                 const gchar      *project)
{
  HyScanObjectDataPrivate *priv;
  HyScanObjectDataClass *klass;
  gint32 project_id = 0;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);
  klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  priv = data->priv;

  if (priv->db != NULL)
    {
      if (priv->param_id > 0)
        hyscan_db_close (priv->db, priv->param_id);
      g_clear_object (&priv->db);
    }
  g_free (priv->project);

  priv->db = g_object_ref (db);
  priv->project = g_strdup (project);

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

  /* Открываем (или создаём) группу параметров. */
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, klass->group_name);
  if (priv->param_id <= 0)
    {
      g_warning ("HyScanObjectData: can't open group %s (project '%s')", klass->group_name, priv->project);
      goto exit;
    }

exit:
  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);

  return priv->param_id > 0;
}

/**
 * hyscan_object_data_is_ready:
 * @data: указатель на объект #HyScanObjectData
 *
 * Проверяет, корректно ли проинициализировался объект. Если нет, то он будет
 * неработоспособен.
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
 * hyscan_object_data_generate_id:
 * @data: указатель на #HyScanObjectData
 * @object: объекта
 *
 * Функция генерирует идентификатор без обращения к базе данных.
 * Функция является потокобезопасной.
 *
 * Returns: (transfer full): новый идентификатор объекта, для удаления g_free().
 */
gchar *
hyscan_object_data_generate_id (HyScanObjectData   *data,
                                const HyScanObject *object)
{
  HyScanObjectDataClass *klass;
  gchar *id;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), NULL);

  klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  if (klass->generate_id != NULL)
    return klass->generate_id (data, object);

  id = g_malloc (OBJECT_ID_LEN);

  return hyscan_rand_id (id, OBJECT_ID_LEN);
}

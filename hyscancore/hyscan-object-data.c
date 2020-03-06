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

static void           hyscan_object_data_set_property          (GObject                *object,
                                                                guint                   prop_id,
                                                                const GValue           *value,
                                                                GParamSpec             *pspec);
static void           hyscan_object_data_get_property          (GObject                *object,
                                                                guint                   prop_id,
                                                                GValue                 *value,
                                                                GParamSpec             *pspec);
static void           hyscan_object_data_object_constructed    (GObject                *object);
static void           hyscan_object_data_object_finalize       (GObject                *object);
static gchar *        hyscan_object_data_generate_id           (HyScanObjectData       *data,
                                                                const HyScanObject     *object);

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
  HyScanObjectData *data = HYSCAN_OBJECT_DATA (object);
  HyScanObjectDataClass *klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);
  HyScanObjectDataPrivate *priv = data->priv;

  gint32 project_id = 0;

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
      g_warning ("HyScanObjectData: can't open group %s (project '%s')",
                 klass->group_name, priv->project);
      goto exit;
    }

  priv->plist = hyscan_param_list_new ();

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

  g_clear_object (&priv->plist);

  G_OBJECT_CLASS (hyscan_object_data_parent_class)->finalize (object);
}

/* Функция генерирует идентификатор. */
static gchar *
hyscan_object_data_generate_id (HyScanObjectData   *data,
                                const HyScanObject *object)
{
  gchar *id;

  id = g_malloc (OBJECT_ID_LEN);

  return hyscan_rand_id (id, OBJECT_ID_LEN);
}

/**
 * hyscan_object_data_new:
 * @type: тип класса, наследник #HyScanObjectData
 * @db: указатель на #HyScanDB
 * @project: имя проекта
 *
 * Создаёт новый объект для работы с параметрами. Для удаления g_object_unref().
 */
HyScanObjectData *
hyscan_object_data_new (GType        type,
                        HyScanDB    *db,
                        const gchar *project)
{
  g_return_val_if_fail (g_type_is_a (type, HYSCAN_TYPE_OBJECT_DATA), NULL);

  return g_object_new (type,
                       "db", db,
                       "project", project,
                       NULL);
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
 * hyscan_object_data_add:
 * @data: указатель на объект #HyScanObjectData
 * @object: указатель на структуру #HyScanObject
 *
 * Функция добавляет объект в базу данных.
 *
 * Returns: %TRUE, если удалось добавить объект, иначе %FALSE.
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

  status = hyscan_object_data_modify (data, id, object);

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
 * @id: идентификатор объекта
 *
 * Функция удаляет объект из базы данных.
 *
 * Returns: %TRUE, если удалось удалить объект, иначе %FALSE.
 */
gboolean
hyscan_object_data_remove (HyScanObjectData *data,
                           const gchar      *id)
{
  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);

  return hyscan_db_param_object_remove (data->priv->db,
                                        data->priv->param_id,
                                        id);
}

/**
 * hyscan_object_data_modify:
 * @data: указатель на объект #HyScanObjectData
 * @id: идентификатор объекта
 * @object: указатель на структуру #HyScanObject
 *
 * Функция изменяет объект.
 *
 * Returns: %TRUE, если удалось изменить объект, иначе %FALSE
 */
gboolean
hyscan_object_data_modify (HyScanObjectData   *data,
                           const gchar        *id,
                           const HyScanObject *object)
{
  HyScanObjectDataPrivate *priv;
  HyScanObjectDataClass *klass;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), FALSE);
  priv = data->priv;
  klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  hyscan_param_list_clear (priv->plist);

  if (klass->set_full != NULL)
    klass->set_full (data, priv->plist, object);

  return hyscan_db_param_set (priv->db, priv->param_id, id, priv->plist);
}

/**
 * hyscan_object_data_get_ids:
 * @data: указатель на объект #HyScanObjectData
 * @len: (optional): количество элементов или %NULL
 *
 * Функция возвращает список идентификаторов всех меток.
 *
 * Returns: (array length=len) (transfer full): %NULL-терминированный список
 *   идентификаторов, %NULL если меток нет. Для удаления g_strfreev()
 */
gchar **
hyscan_object_data_get_ids (HyScanObjectData *data,
                            guint            *len)
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
 * @id: идентификатор объекта
 *
 * Функция возвращает объект по идентификатору.
 *
 * Returns: (transfer full): новая структура #HyScanObject, %NULL в случае ошибки. Для
 *   удаления HyScanObjectDataClass.destroy().
 */
HyScanObject *
hyscan_object_data_get (HyScanObjectData *data,
                        const gchar      *id)
{
  HyScanObjectDataPrivate *priv;
  HyScanObjectDataClass *klass;

  HyScanParamList *read_plist = NULL;
  gboolean read_status;
  HyScanObject *object = NULL;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_DATA (data), NULL);
  priv = data->priv;
  klass = HYSCAN_OBJECT_DATA_GET_CLASS (data);

  g_return_val_if_fail (klass->get_full != NULL && klass->get_read_plist != NULL, NULL);

  /* Получаем список параметров для чтения объекта с этим идентификатором. */
  read_plist = klass->get_read_plist (data, id);

  /* Считываем параметры объекта и формируем из него структуру. */
  read_status = hyscan_db_param_get (priv->db, priv->param_id, id, read_plist);
  if (!read_status)
    goto exit;

  object = klass->get_full (data, read_plist);

exit:
  g_clear_object (&read_plist);

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

/**
 * hyscan_object_copy:
 * @object: указатель на структуру #HyScanObject
 *
 * Функция создаёт структуру с копией указанного объекта @object.
 *
 * Returns: (transfer full): копия структуры или %NULL
 */
HyScanObject *
hyscan_object_copy (const HyScanObject *object)
{
  if (object == NULL)
    return NULL;

  return g_boxed_copy (object->type, object);
}

/**
 * hyscan_object_free:
 * @object: указатель на структуру #HyScanObject
 *
 * Функция удаляет структуру @object.
 */
void
hyscan_object_free (HyScanObject *object)
{
  if (object == NULL)
    return;

  g_boxed_free (object->type, object);
}

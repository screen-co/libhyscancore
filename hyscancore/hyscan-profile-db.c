/*
 * \file hyscan-profile-db.c
 *
 * \brief Исходный файл класса HyScanProfileDB - профиля БД.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-profile-db.h"

#define HYSCAN_PROFILE_DB_GROUP_NAME "db"
#define HYSCAN_PROFILE_DB_URI_KEY "uri"
#define HYSCAN_PROFILE_DB_NAME_KEY "name"
#define HYSCAN_PROFILE_DB_UNNAMED_VALUE "unnamed"

enum
{
  PROP_O,
  PROP_URI,
  PROP_NAME
};

struct _HyScanProfileDBPrivate
{
  gchar *name;   /* Имя БД. */
  gchar *uri;    /* Идентификатор БД.*/
};

static void   hyscan_profile_db_set_property    (GObject                      *object,
                                                 guint                         prop_id,
                                                 const GValue                 *value,
                                                 GParamSpec                   *pspec);
static void   hyscan_profile_db_get_property    (GObject                      *object,
                                                 guint                         prop_id,
                                                 GValue                       *value,
                                                 GParamSpec                   *pspec);
static void   hyscan_profile_db_object_finalize (GObject                      *object);
static void   hyscan_profile_db_clear           (HyScanProfileDB              *profile);
static gboolean hyscan_profile_db_read          (HyScanProfile                *profile,
                                                 GKeyFile                     *file);

G_DEFINE_TYPE_WITH_CODE (HyScanProfileDB, hyscan_profile_db, HYSCAN_TYPE_PROFILE,
                         G_ADD_PRIVATE (HyScanProfileDB));

static void
hyscan_profile_db_class_init (HyScanProfileDBClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->set_property = hyscan_profile_db_set_property;
  oclass->get_property = hyscan_profile_db_get_property;
  oclass->finalize = hyscan_profile_db_object_finalize;

  pklass->read = hyscan_profile_db_read;

  g_object_class_install_property (oclass, PROP_URI,
    g_param_spec_string ("uri", "URI", "Database URI",
                         NULL, G_PARAM_READWRITE));

  g_object_class_install_property (oclass, PROP_NAME,
    g_param_spec_string ("name", "Name", "Database name",
                         NULL, G_PARAM_READWRITE));
}

static void
hyscan_profile_db_init (HyScanProfileDB *profile)
{
  profile->priv = hyscan_profile_db_get_instance_private (profile);
}

static void
hyscan_profile_db_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanProfileDB *profile = HYSCAN_PROFILE_DB (object);

  switch (prop_id)
    {
    case PROP_URI:
      hyscan_profile_db_set_uri (profile, g_value_get_string (value));
      break;

    case PROP_NAME:
      hyscan_profile_db_set_name (profile, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_profile_db_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  HyScanProfileDB *profile = HYSCAN_PROFILE_DB (object);
  HyScanProfileDBPrivate *priv = profile->priv;

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_profile_db_object_finalize (GObject *object)
{
  HyScanProfileDB *self = HYSCAN_PROFILE_DB (object);

  hyscan_profile_db_clear (self);

  G_OBJECT_CLASS (hyscan_profile_db_parent_class)->finalize (object);
}

static void
hyscan_profile_db_clear (HyScanProfileDB *profile)
{
  HyScanProfileDBPrivate *priv = profile->priv;

  g_clear_pointer (&priv->uri, g_free);
  g_clear_pointer (&priv->name, g_free);
}

/* Создаёт объект HyScanProfileDB. */
HyScanProfileDB *
hyscan_profile_db_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_DB,
                       "file", file,
                       NULL);
}

/* Десериализация из INI-файла. */
static gboolean
hyscan_profile_db_read (HyScanProfile *profile,
                        GKeyFile      *file)
{
  HyScanProfileDB *self = HYSCAN_PROFILE_DB (profile);
  HyScanProfileDBPrivate *priv = self->priv;

  /* Очистка профиля. */
  hyscan_profile_db_clear (self);

  priv->name = g_key_file_get_string (file, HYSCAN_PROFILE_DB_GROUP_NAME, HYSCAN_PROFILE_DB_NAME_KEY, NULL);
  priv->uri = g_key_file_get_string (file, HYSCAN_PROFILE_DB_GROUP_NAME, HYSCAN_PROFILE_DB_URI_KEY, NULL);
  if (priv->uri == NULL)
    {
      g_warning ("HyScanProfileDB: %s", "uri not found.");
      return FALSE;
    }

  hyscan_profile_set_name (profile, priv->name != NULL ? priv->name : priv->uri);

  return TRUE;
}

/* Получает имя системы хранения. */
const gchar *
hyscan_profile_db_get_name (HyScanProfileDB *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_DB (profile), NULL);

  return profile->priv->name;
}

/* Получает URI. */
const gchar *
hyscan_profile_db_get_uri (HyScanProfileDB *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_DB (profile), NULL);

  return profile->priv->uri;
}

/* Задаёт имя системы хранения. */
void
hyscan_profile_db_set_name (HyScanProfileDB *profile,
                            const gchar     *name)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_DB (profile));

  g_free (profile->priv->name);
  profile->priv->name = g_strdup (name);
}

/* Задаёт URI. */
void
hyscan_profile_db_set_uri (HyScanProfileDB *profile,
                           const gchar     *uri)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_DB (profile));

  g_free (profile->priv->uri);
  profile->priv->uri = g_strdup (uri);
}

HyScanDB *
hyscan_profile_db_connect (HyScanProfileDB *profile)
{
  HyScanProfileDBPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_DB (profile), NULL);
  priv = profile->priv;

  if (priv->uri == NULL)
    {
      g_warning ("HyScanProfileDB: %s", "uri not set");
      return NULL;
    }

  return hyscan_db_new (priv->uri);
}

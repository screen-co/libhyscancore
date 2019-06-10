/*
 * \file hyscan-db-profile.c
 *
 * \brief Исходный файл класса HyScanProfileOffset - профиля БД.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <gio/gio.h>
#include "hyscan-profile-offset.h"

#define HYSCAN_PROFILE_OFFSET_INFO_GROUP "_"
#define HYSCAN_PROFILE_OFFSET_NAME "name"

#define HYSCAN_PROFILE_OFFSET_X "x"
#define HYSCAN_PROFILE_OFFSET_Y "y"
#define HYSCAN_PROFILE_OFFSET_Z "z"
#define HYSCAN_PROFILE_OFFSET_PSI "psi"
#define HYSCAN_PROFILE_OFFSET_GAMMA "gamma"
#define HYSCAN_PROFILE_OFFSET_THETA "theta"

struct _HyScanProfileOffsetPrivate
{
  GHashTable *sonars;  /* Таблица смещений для локаторов. {HyScanSourceType : HyScanAntennaOffset} */
  GHashTable *sensors; /* Таблица смещений для антенн. {gchar* : HyScanAntennaOffset} */
};

static void     hyscan_profile_offset_object_finalize  (GObject             *object);
static void     hyscan_profile_offset_clear            (HyScanProfileOffset *profile);
static gboolean hyscan_profile_offset_read             (HyScanProfile       *profile,
                                                        GKeyFile            *file);
static gboolean hyscan_profile_offset_info_group       (HyScanProfileOffset *profile,
                                                        GKeyFile            *kf,
                                                        const gchar         *group);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileOffset, hyscan_profile_offset, HYSCAN_TYPE_PROFILE);

static void
hyscan_profile_offset_class_init (HyScanProfileOffsetClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_profile_offset_object_finalize;
  pklass->read = hyscan_profile_offset_read;
}

static void
hyscan_profile_offset_init (HyScanProfileOffset *profile)
{
  profile->priv = hyscan_profile_offset_get_instance_private (profile);
}

static void
hyscan_profile_offset_object_finalize (GObject *object)
{
  HyScanProfileOffset *self = HYSCAN_PROFILE_OFFSET (object);

  hyscan_profile_offset_clear (self);

  G_OBJECT_CLASS (hyscan_profile_offset_parent_class)->finalize (object);
}

static void
hyscan_profile_offset_clear (HyScanProfileOffset *profile)
{
  HyScanProfileOffsetPrivate *priv = profile->priv;

  g_clear_pointer (&priv->sonars, g_hash_table_unref);
  g_clear_pointer (&priv->sensors, g_hash_table_unref);

  priv->sonars = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL, (GDestroyNotify)hyscan_antenna_offset_free);
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify)hyscan_antenna_offset_free);
}

/* Создаёт объект HyScanProfileOffset. */
HyScanProfileOffset *
hyscan_profile_offset_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_OFFSET,
                       "file", file,
                       NULL);
}

/* Десериализация из INI-файла. */
static gboolean
hyscan_profile_offset_read (HyScanProfile *profile,
                            GKeyFile      *file)
{
  HyScanProfileOffset *self = HYSCAN_PROFILE_OFFSET (profile);
  HyScanProfileOffsetPrivate *priv = self->priv;
  gchar **groups, **iter;

  /* Очищаем, если что-то было. */
  hyscan_profile_offset_clear (self);

  groups = g_key_file_get_groups (file, NULL);
  for (iter = groups; iter != NULL && *iter != NULL; ++iter)
    {
      HyScanAntennaOffset offset;
      HyScanSourceType source;
      HyScanChannelType type;
      guint channel;

      /* Возможно, это группа с информацией. */
      if (hyscan_profile_offset_info_group (self, file, *iter))
        continue;

      offset.x = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_X, NULL);
      offset.y = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_Y, NULL);
      offset.z = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_Z, NULL);
      offset.psi = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_PSI, NULL);
      offset.gamma = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_GAMMA, NULL);
      offset.theta = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_THETA, NULL);

      /* Если название группы совпадает с названием того или иного
       * HyScanSourceType, то это локатор. Иначе -- датчик. */
      if (hyscan_channel_get_types_by_id (*iter, &source, &type, &channel))
        g_hash_table_insert (priv->sonars, GINT_TO_POINTER (source), hyscan_antenna_offset_copy (&offset));
      else
        g_hash_table_insert (priv->sensors, g_strdup (*iter), hyscan_antenna_offset_copy (&offset));
    }

  g_strfreev (groups);

  return TRUE;
}

static gboolean
hyscan_profile_offset_info_group (HyScanProfileOffset *profile,
                                  GKeyFile            *kf,
                                  const gchar         *group)
{
  gchar *name;

  if (!g_str_equal (group, HYSCAN_PROFILE_OFFSET_INFO_GROUP))
    return FALSE;

  name = g_key_file_get_string (kf, group, HYSCAN_PROFILE_OFFSET_NAME, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), name);

  g_free (name);
  return TRUE;
}

GHashTable *
hyscan_profile_offset_get_sonars (HyScanProfileOffset *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), NULL);

  return g_hash_table_ref (profile->priv->sonars);
}

GHashTable *
hyscan_profile_offset_get_sensors (HyScanProfileOffset *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), NULL);

  return g_hash_table_ref (profile->priv->sensors);
}

gboolean
hyscan_profile_offset_apply (HyScanProfileOffset *profile,
                             HyScanControl       *control)
{
  gpointer k;
  HyScanAntennaOffset *v;
  GHashTableIter iter;
  HyScanProfileOffsetPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), FALSE);
  priv = profile->priv;

  g_hash_table_iter_init (&iter, priv->sonars);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (control), (HyScanSourceType)k, v))
        return FALSE;
    }

  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (control), (const gchar*)k, v))
        return FALSE;
    }

  return TRUE;
}

gboolean
hyscan_profile_offset_apply_default (HyScanProfileOffset *profile,
                                     HyScanControl       *control)
{
  gpointer k;
  HyScanAntennaOffset *v;
  GHashTableIter iter;
  HyScanProfileOffsetPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), FALSE);
  priv = profile->priv;

  g_hash_table_iter_init (&iter, priv->sonars);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_control_source_set_default_offset (control, (HyScanSourceType)k, v))
        return FALSE;
    }

  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_control_sensor_set_default_offset (control, (const gchar*)k, v))
        return FALSE;
    }

  return TRUE;
}

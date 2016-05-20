/*
 * \file hyscan-data-channel-writer.c
 *
 * \brief Исходный файл класса обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-data-channel-writer.h"
#include "hyscan-convolution.h"

#include <math.h>
#include <string.h>

/* Название схем данных для каналов данных и сигналов. */
#define DATA_CHANNEL_SCHEMA                    "data"
#define SIGNAL_CHANNEL_SCHEMA                  "signal"

/* Постфикс названия канала с образцами сигналов. */
#define SIGNALS_CHANNEL_POSTFIX                "signals"

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_CHANNEL_NAME,
  PROP_CHANNEL_INFO
};

struct _HyScanDataChannelWriterPrivate
{
  HyScanDB            *db;                             /* Интерфейс базы данных. */

  gchar               *project_name;                   /* Название проекта. */
  gchar               *track_name;                     /* Название галса. */
  gchar               *channel_name;                   /* Название канала данных. */
  gchar               *signals_name;                   /* Название канала данных с образцами сигналов. */

  HyScanDataChannelInfo info;                          /* Параметры канала данных. */

  gint32               track_id;                       /* Идентификатор открытого галса. */
  gint32               channel_id;                     /* Идентификатор открытого канала данных. */
  gint32               signal_id;                      /* Идентификатор открытого канала с образцами сигналов. */

  gboolean             save_signal;                    /* Признак записи сигналов. */
};

static void     hyscan_data_channel_writer_set_property        (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void     hyscan_data_channel_writer_object_constructed  (GObject                       *object);
static void     hyscan_data_channel_writer_object_finalize     (GObject                       *object);

static gboolean hyscan_data_channel_writer_save_data_params    (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                HyScanDataChannelInfo         *info);

static gboolean hyscan_data_channel_writer_save_signals_params (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                gfloat                         discretization_frequency);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDataChannelWriter, hyscan_data_channel_writer, G_TYPE_OBJECT);

static void
hyscan_data_channel_writer_class_init (HyScanDataChannelWriterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_data_channel_writer_set_property;

  object_class->constructed = hyscan_data_channel_writer_object_constructed;
  object_class->finalize = hyscan_data_channel_writer_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CHANNEL_NAME,
    g_param_spec_string ("channel-name", "ChannelName", "Channel name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CHANNEL_INFO,
    g_param_spec_pointer ("channel-info", "ChannelInfo", "Channel info",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_data_channel_writer_init (HyScanDataChannelWriter *dwriter)
{
  dwriter->priv = hyscan_data_channel_writer_get_instance_private (dwriter);
}

static void
hyscan_data_channel_writer_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec )
{
  HyScanDataChannelWriter *dwriter = HYSCAN_DATA_CHANNEL_WRITER (object);
  HyScanDataChannelWriterPrivate *priv = dwriter->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project_name = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track_name = g_value_dup_string (value);
      break;

    case PROP_CHANNEL_NAME:
      priv->channel_name = g_value_dup_string (value);
      priv->signals_name = g_strdup_printf ("%s.%s", priv->channel_name, SIGNALS_CHANNEL_POSTFIX);
      break;

    case PROP_CHANNEL_INFO:
      {
        HyScanDataChannelInfo *info = g_value_get_pointer (value);
        if (info != NULL)
          memcpy (&priv->info, info, sizeof (HyScanDataChannelInfo));
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (dwriter, prop_id, pspec);
      break;
    }
}

static void
hyscan_data_channel_writer_object_constructed (GObject *object)
{
  HyScanDataChannelWriter *dwriter = HYSCAN_DATA_CHANNEL_WRITER (object);
  HyScanDataChannelWriterPrivate *priv = dwriter->priv;

  gint32 project_id = -1;
  gint32 param_id = -1;

  gboolean status = FALSE;

  priv->channel_id = -1;
  priv->signal_id = -1;

  if (priv->db == NULL)
    goto exit;

  /* Названия проекта, галса, канала данных. */
  if (priv->project_name == NULL || priv->track_name == NULL|| priv->channel_name == NULL)
    {
      if (priv->project_name == NULL)
        g_warning ("HyScanDataChannelWriter: unknown project name");
      if (priv->track_name == NULL)
        g_warning ("HyScanDataChannelWriter: unknown track name");
      if (priv->channel_name == NULL)
        g_warning ("HyScanDataChannelWriter: unknown channel name");
      goto exit;
    }

  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    {
      g_warning ("HyScanDataChannelWriter: can't open project '%s'",
                 priv->project_name);
      goto exit;
    }

  priv->track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (priv->track_id < 0)
    {
      g_warning ("HyScanDataChannelWriter: can't open track '%s.%s'",
                 priv->project_name, priv->track_name);
      goto exit;
    }

  priv->channel_id = hyscan_db_channel_create (priv->db, priv->track_id, priv->channel_name, DATA_CHANNEL_SCHEMA);
  if (priv->channel_id < 0)
    {
      g_warning ("HyScanDataChannelWriter: can't create channel '%s.%s.%s'",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanDataChannelWriter: can't open channel '%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  if (!hyscan_data_channel_writer_save_data_params (priv->db, param_id, &priv->info))
    {
      g_warning ("HyScanDataChannelWriter: can't save channel '%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  /* Образцы сигналов для свёртки. */
  priv->signal_id = hyscan_db_channel_create (priv->db, priv->track_id, priv->signals_name, SIGNAL_CHANNEL_SCHEMA);

  /* Параметры канала с образцами сигналов. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->signal_id);
  if (param_id < 0)
    {
      g_warning ("HyScanDataChannelWriter: can't open channel '%s.%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name,
                 SIGNALS_CHANNEL_POSTFIX);
      goto exit;
    }

  if (!hyscan_data_channel_writer_save_signals_params (priv->db, param_id, priv->info.discretization_frequency))
    {
      g_warning ("HyScanDataChannel: can't save signals '%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  status = TRUE;

exit:
  if (!status)
    {
      if (priv->channel_id > 0)
        hyscan_db_close (priv->db, priv->channel_id);
      if (priv->signal_id > 0)
        hyscan_db_close (priv->db, priv->signal_id);
      priv->channel_id = -1;
      priv->signal_id = -1;
    }

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  if (param_id > 0)
    hyscan_db_close (priv->db, param_id);
}

static void
hyscan_data_channel_writer_object_finalize (GObject *object)
{
  HyScanDataChannelWriter *dwriter = HYSCAN_DATA_CHANNEL_WRITER (object);
  HyScanDataChannelWriterPrivate *priv = dwriter->priv;

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);
  if (priv->signal_id > 0)
    hyscan_db_close (priv->db, priv->signal_id);

  /* Если не записывали сигналы, удаляем соответствующий канал данных. */
  if (!priv->save_signal)
    hyscan_db_channel_remove (priv->db, priv->track_id, priv->signals_name);

  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->channel_name);
  g_free (priv->signals_name);

  g_clear_object (&priv->db);

  G_OBJECT_CLASS (hyscan_data_channel_writer_parent_class)->finalize (object);
}

/* Функция сохраняет параметры канала данных. */
static gboolean
hyscan_data_channel_writer_save_data_params (HyScanDB              *db,
                                             gint32                 param_id,
                                             HyScanDataChannelInfo *info)
{
  const gchar *discretization_type;

  discretization_type = hyscan_data_get_type_name (info->discretization_type);
  if (!hyscan_db_param_set_string (db, param_id, NULL, "/discretization/type", discretization_type))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/discretization/frequency", info->discretization_frequency))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/pattern/vertical", info->vertical_pattern))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/pattern/horizontal", info->horizontal_pattern))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/x", info->x))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/y", info->y))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/z", info->z))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/orientation/psi", info->psi))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/orientation/gamma", info->gamma))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/orientation/theta", info->theta))
    return FALSE;

  return TRUE;
}

/* Функция сохраняет параметры канала с образцами сигналов. */
static gboolean
hyscan_data_channel_writer_save_signals_params (HyScanDB *db,
                                                gint32    param_id,
                                                gfloat    discretization_frequency)
{
  const gchar *discretization_type;

  discretization_type = hyscan_data_get_type_name (HYSCAN_DATA_COMPLEX_FLOAT);
  if (!hyscan_db_param_set_string (db, param_id, NULL, "/discretization/type", discretization_type))
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/discretization/frequency", discretization_frequency))
    return FALSE;

  return TRUE;
}

/* Функция создаёт новый объект первичной обработки акустических данных. */
HyScanDataChannelWriter *
hyscan_data_channel_writer_new (HyScanDB              *db,
                                const gchar           *project_name,
                                const gchar           *track_name,
                                const gchar           *channel_name,
                                HyScanDataChannelInfo *channel_info)
{
  return g_object_new (HYSCAN_TYPE_DATA_CHANNEL_WRITER,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "channel-name", channel_name,
                       "channel-info", channel_info,
                       NULL);
}

/* Функция задаёт максимальный размер файлов, хранящих данные канала. */
gboolean
hyscan_data_channel_writer_set_chunk_size (HyScanDataChannelWriter *dwriter,
                                           gint32                   chunk_size)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL_WRITER (dwriter), FALSE);

  if (dwriter->priv->channel_id < 0)
    return FALSE;

  return hyscan_db_channel_set_chunk_size (dwriter->priv->db, dwriter->priv->channel_id, chunk_size);
}

/* Функция задаёт интервал времени, для которого сохраняются записываемые данные. */
gboolean
hyscan_data_channel_writer_set_save_time (HyScanDataChannelWriter *dwriter,
                                          gint64                   save_time)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL_WRITER (dwriter), FALSE);

  if (dwriter->priv->channel_id < 0)
    return FALSE;

  return hyscan_db_channel_set_save_time (dwriter->priv->db, dwriter->priv->channel_id, save_time);
}

/* Функция задаёт объём сохраняемых данных в канале. */
gboolean
hyscan_data_channel_writer_set_save_size (HyScanDataChannelWriter *dwriter,
                                          gint64                   save_size)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL_WRITER (dwriter), FALSE);

  if (dwriter->priv->channel_id < 0)
    return FALSE;

  return hyscan_db_channel_set_save_size (dwriter->priv->db, dwriter->priv->channel_id, save_size);
}

/* Функция задаёт образец сигнала для свёртки. */
gboolean
hyscan_data_channel_writer_add_signal_image (HyScanDataChannelWriter  *dwriter,
                                             gint64                    time,
                                             HyScanComplexFloat       *image,
                                             gint32                    n_points)
{
  HyScanDataChannelWriterPrivate *priv;

  gint32 signal_size;
  HyScanComplexFloat zero = {0.0, 0.0};

  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL_WRITER (dwriter), FALSE);

  priv = dwriter->priv;

  if (priv->signal_id < 0)
    return FALSE;

  /* Признак отключения свёртки с указанного момента времени. */
  if (image == NULL || n_points == 0)
    {
      image = &zero;
      signal_size = sizeof (zero);
    }
  else
    {
      signal_size = n_points * sizeof (HyScanComplexFloat);
    }

  priv->save_signal = TRUE;

  return hyscan_db_channel_add_data (priv->db, priv->signal_id, time, image, signal_size, NULL);
}

/* Функция записывает новые данные в канал. */
gboolean
hyscan_data_channel_writer_add_data (HyScanDataChannelWriter *dwriter,
                                     gint64                   time,
                                     gconstpointer            data,
                                     gint32                   size)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL_WRITER (dwriter), FALSE);

  if (dwriter->priv->channel_id < 0)
    return FALSE;

  return hyscan_db_channel_add_data (dwriter->priv->db, dwriter->priv->channel_id, time, data, size, NULL);
}

/*
 * \file hyscan-fl-gen.c
 *
 * \brief Исходный файл класса генерации тестовых данных вперёдсмотрящего локатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-fl-gen.h"

#include <hyscan-data-writer.h>

#include <math.h>

struct _HyScanFLGenPrivate
{
  HyScanDataWriter            *writer;

  HyScanAntennaPosition        position;

  HyScanRawDataInfo            info1;
  HyScanRawDataInfo            info2;

  HyScanBuffer                *values1;
  HyScanBuffer                *values2;
};

static void    hyscan_fl_gen_object_finalize           (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanFLGen, hyscan_fl_gen, G_TYPE_OBJECT)

static void
hyscan_fl_gen_class_init (HyScanFLGenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_fl_gen_object_finalize;
}

static void
hyscan_fl_gen_init (HyScanFLGen *fl_gen)
{
  fl_gen->priv = hyscan_fl_gen_get_instance_private (fl_gen);

  fl_gen->priv->values1 = hyscan_buffer_new ();
  fl_gen->priv->values2 = hyscan_buffer_new ();

  hyscan_buffer_set_data_type (fl_gen->priv->values1, HYSCAN_DATA_COMPLEX_FLOAT);
  hyscan_buffer_set_data_type (fl_gen->priv->values2, HYSCAN_DATA_COMPLEX_FLOAT);
}

static void
hyscan_fl_gen_object_finalize (GObject *object)
{
  HyScanFLGen *fl_gen = HYSCAN_FL_GEN (object);
  HyScanFLGenPrivate *priv = fl_gen->priv;

  g_clear_object (&priv->writer);

  g_clear_object (&priv->values1);
  g_clear_object (&priv->values2);

  G_OBJECT_CLASS (hyscan_fl_gen_parent_class)->finalize (object);
}

HyScanFLGen *
hyscan_fl_gen_new (void)
{
  return g_object_new (HYSCAN_TYPE_FL_GEN, NULL);
}

void
hyscan_fl_gen_set_position (HyScanFLGen           *fl_gen,
                            HyScanAntennaPosition *position)
{
  g_return_if_fail (HYSCAN_IS_FL_GEN (fl_gen));

  fl_gen->priv->position = *position;
}

void
hyscan_fl_gen_set_info (HyScanFLGen       *fl_gen,
                        HyScanRawDataInfo *info)
{
  g_return_if_fail (HYSCAN_IS_FL_GEN (fl_gen));

  fl_gen->priv->info1 = *info;
  fl_gen->priv->info2 = *info;

  fl_gen->priv->info1.data_type = HYSCAN_DATA_COMPLEX_FLOAT;
  fl_gen->priv->info2.data_type = HYSCAN_DATA_COMPLEX_FLOAT;

  fl_gen->priv->info1.antenna_hoffset = 0.0;
  fl_gen->priv->info2.antenna_hoffset = 0.01;
}

gboolean
hyscan_fl_gen_set_track (HyScanFLGen *fl_gen,
                         HyScanDB    *db,
                         const gchar *project_name,
                         const gchar *track_name)
{
  HyScanFLGenPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FL_GEN (fl_gen), FALSE);

  priv = fl_gen->priv;

  g_clear_object (&priv->writer);

  priv->writer = hyscan_data_writer_new (db);

  hyscan_data_writer_sonar_set_position (priv->writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &priv->position);

  if (!hyscan_data_writer_start (priv->writer, project_name, track_name, HYSCAN_TRACK_SURVEY))
    return FALSE;

  return TRUE;
}

gboolean
hyscan_fl_gen_generate (HyScanFLGen *fl_gen,
                        guint32      n_points,
                        gint64       time)
{
  HyScanFLGenPrivate *priv;

  HyScanComplexFloat *raw_values1;
  HyScanComplexFloat *raw_values2;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_FL_GEN (fl_gen), FALSE);

  priv = fl_gen->priv;

  if (priv->writer == NULL)
    return FALSE;

  hyscan_buffer_set_size (priv->values1, n_points * sizeof (HyScanComplexFloat));
  hyscan_buffer_set_size (priv->values2, n_points * sizeof (HyScanComplexFloat));

  raw_values1 = hyscan_buffer_get_complex_float (priv->values1, &n_points);
  raw_values2 = hyscan_buffer_get_complex_float (priv->values2, &n_points);

  /* Тестовые данные для проверки работы вперёдсмотрящего локатора. В каждой строке
   * разность фаз между двумя каналами изменяется в пределах Pi по дальности, с начальным
   * отклонением зависящим от текущего времени. */
  for (i = 0; i < n_points; i++)
    {
      gdouble mini_pi;
      gdouble phase;

      mini_pi = 0.999 * G_PI;
      phase  = mini_pi;
      phase -= mini_pi * ((gdouble)i / (gdouble)(n_points - 1));
      phase -= mini_pi * ((gdouble)(time % 1000000) / 999999.0);

      raw_values1[i].re = 1.0;
      raw_values1[i].im = 0.0;
      raw_values2[i].re = cos (phase);
      raw_values2[i].im = sin (phase);
    }

  if (!hyscan_data_writer_raw_add_data (priv->writer, HYSCAN_SOURCE_FORWARD_LOOK, 1, time, &priv->info1, priv->values1))
    return FALSE;

  if (!hyscan_data_writer_raw_add_data (priv->writer, HYSCAN_SOURCE_FORWARD_LOOK, 2, time, &priv->info2, priv->values2))
    return FALSE;

  return TRUE;
}

gboolean
hyscan_fl_gen_check (const HyScanForwardLookDOA *doa,
                     guint32                     n_points,
                     gint64                      time,
                     gdouble                     alpha)
{
  gdouble max_distance = doa[n_points - 1].distance;
  guint i;

  for (i = 0; i < n_points; i++)
    {
      gdouble angle;
      gdouble distance;

      angle = -alpha;
      angle += alpha * ((gdouble)i / (gdouble)(n_points - 1));
      angle += alpha * ((gdouble)(time % 1000000) / 999999.0);
      distance = max_distance * ((gdouble)i / (gdouble)(n_points - 1));

      if ((fabs (doa[i].distance - distance) > 0.01) ||
          (fabs (doa[i].angle - angle) > 0.01) ||
          (fabs (doa[i].amplitude - 1.0) > 0.01))
        {
          return FALSE;
        }
    }

  return TRUE;
}

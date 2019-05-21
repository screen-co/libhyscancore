/* hyscan-fl-gen.c
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * SECTION: hyscan-fl-gen
 * @Short_description: класс генерации тестовых данных вперёдсмотрящего локатора
 * @Title: HyScanFLGen
 *
 * Класс предназначен для записи тестовых данных вперёдсмотрящего локатора.
 *
 * Тестовые данные представляют собой отражение от целей по всей дистанции
 * приёма с изменением направления от крайнего левого до крайнего правого.
 *
 */

#include "hyscan-fl-gen.h"

#include <hyscan-data-writer.h>

#include <math.h>

struct _HyScanFLGenPrivate
{
  HyScanDataWriter            *writer;

  HyScanAntennaOffset          offset;

  HyScanAcousticDataInfo       info1;
  HyScanAcousticDataInfo       info2;

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

/* Функция создаёт новый объект HyScanFLGen. */
HyScanFLGen *
hyscan_fl_gen_new (void)
{
  return g_object_new (HYSCAN_TYPE_FL_GEN, NULL);
}

/* Функция задаёт смещение приёмной антенны локатора. */
void
hyscan_fl_gen_set_offset (HyScanFLGen         *fl_gen,
                          HyScanAntennaOffset *offset)
{
  g_return_if_fail (HYSCAN_IS_FL_GEN (fl_gen));

  fl_gen->priv->offset = *offset;
}

/* Функция задаёт параметры локатора. */
void
hyscan_fl_gen_set_info (HyScanFLGen            *fl_gen,
                        HyScanAcousticDataInfo *info)
{
  g_return_if_fail (HYSCAN_IS_FL_GEN (fl_gen));

  fl_gen->priv->info1 = *info;
  fl_gen->priv->info2 = *info;

  fl_gen->priv->info1.data_type = HYSCAN_DATA_COMPLEX_FLOAT;
  fl_gen->priv->info2.data_type = HYSCAN_DATA_COMPLEX_FLOAT;

  fl_gen->priv->info1.antenna_hoffset = 0.0;
  fl_gen->priv->info2.antenna_hoffset = 0.01;
}

/* Функция задаёт проект и галс в который ведётся запись. */
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

  priv->writer = hyscan_data_writer_new ();

  hyscan_data_writer_set_db (priv->writer, db);
  hyscan_data_writer_sonar_set_offset (priv->writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &priv->offset);

  if (!hyscan_data_writer_start (priv->writer, project_name, track_name, HYSCAN_TRACK_SURVEY, -1))
    return FALSE;

  return TRUE;
}

/* Функция генерирует и записывает в галс одну строку данных. */
gboolean
hyscan_fl_gen_generate (HyScanFLGen *fl_gen,
                        guint32      n_points,
                        gint64       time)
{
  HyScanFLGenPrivate *priv;

  HyScanComplexFloat *raw_values1;
  HyScanComplexFloat *raw_values2;

  gboolean status;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_FL_GEN (fl_gen), FALSE);

  priv = fl_gen->priv;

  if (priv->writer == NULL)
    return FALSE;

  hyscan_buffer_set_complex_float (priv->values1, NULL, n_points);
  hyscan_buffer_set_complex_float (priv->values2, NULL, n_points);

  raw_values1 = hyscan_buffer_get_complex_float (priv->values1, &n_points);
  raw_values2 = hyscan_buffer_get_complex_float (priv->values2, &n_points);

  /* Тестовые данные для проверки работы вперёдсмотрящего локатора. В каждой строке
   * разность фаз между двумя каналами изменяется в пределах Pi по дальности, с начальным
   * отклонением зависящим от текущего времени. */
  for (i = 0; i < n_points; i++)
    {
      gdouble phase;

      phase  = G_PI;
      phase -= G_PI * ((gdouble)i / (gdouble)(n_points - 1));
      phase -= G_PI * ((gdouble)(time % 1000000) / 999999.0);

      raw_values1[i].re = 1.0;
      raw_values1[i].im = 0.0;
      raw_values2[i].re = cos (phase);
      raw_values2[i].im = sin (phase);
    }

  status = hyscan_data_writer_acoustic_add_data (priv->writer, HYSCAN_SOURCE_FORWARD_LOOK, 1,
                                                 FALSE, time, &priv->info1, priv->values1);
  if (!status)
    return FALSE;

  status = hyscan_data_writer_acoustic_add_data (priv->writer, HYSCAN_SOURCE_FORWARD_LOOK, 2,
                                                 FALSE, time, &priv->info2, priv->values2);
  if (!status)
    return FALSE;

  return TRUE;
}

/* Функция проверяет валидность данных. */
gboolean
hyscan_fl_gen_check (const HyScanDOA *doa,
                     guint32          n_points,
                     gint64           time,
                     gdouble          alpha)
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

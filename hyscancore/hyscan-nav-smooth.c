/* hyscan-nav-smooth.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-nav-smooth
 * @Short_description: Интерполяция навигационных данных
 * @Title: HyScanNavSmooth
 * @See_also: #HyScanNavData
 *
 * Класс #HyScanNavSmooth предназначен для получения значений навигационных данных
 * на указанный момент времени путём интерполяции дискретных данных из HyScanNavData.
 *
 * Интерполированное значение получается как средневзешанное двух ближайших
 * по времени значений.
 *
 * Для работы с угловыми величинами, такими как курс, необходимо использовать
 * экземляр #HyScanNavSmooth, созданнный при помощи hyscan_nav_smooth_circular_new().
 *
 * Функции:
 * - hyscan_nav_smooth_new() - создание объекта;
 * - hyscan_nav_smooth_new_circular() - создание объекта для угловых величин;
 * - hyscan_nav_smooth_get() - получение интерполированное значение;
 * - hyscan_nav_smooth_get_data() - получение источника навигационных данных.
 *
 */

#include "hyscan-nav-smooth.h"
#include <math.h>

enum
{
  PROP_O,
  PROP_CIRCULAR,
  PROP_NAV_DATA,
};

struct _HyScanNavSmoothPrivate
{
  HyScanNavData       *nav_data;  /* Источник навигационных данных. */
  gboolean             circular;  /* Признак обработки круговых величин. */
};

static void           hyscan_nav_smooth_set_property          (GObject               *object,
                                                               guint                  prop_id,
                                                               const GValue          *value,
                                                               GParamSpec            *pspec);
static void           hyscan_nav_smooth_object_constructed    (GObject               *object);
static void           hyscan_nav_smooth_object_finalize       (GObject               *object);
static inline gdouble hyscan_nav_smooth_weight                (gint64                time,
                                                               gint64                ltime,
                                                               gint64                rtime,
                                                               gdouble               lvalue,
                                                               gdouble               rvalue);
static inline gdouble hyscan_nav_smooth_weight_circular       (gint64                time,
                                                               gint64                ltime,
                                                               gint64                rtime,
                                                               gdouble               lvalue,
                                                               gdouble               rvalue);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNavSmooth, hyscan_nav_smooth, G_TYPE_OBJECT)

static void
hyscan_nav_smooth_class_init (HyScanNavSmoothClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nav_smooth_set_property;

  object_class->constructed = hyscan_nav_smooth_object_constructed;
  object_class->finalize = hyscan_nav_smooth_object_finalize;

  g_object_class_install_property (object_class, PROP_NAV_DATA,
    g_param_spec_object ("nav-data", "Nav Data", "HyScanNavData source to smooth", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CIRCULAR,
    g_param_spec_boolean ("circular", "Circular mean", "Calculate circular mean in [0, 360)", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_nav_smooth_init (HyScanNavSmooth *nav_smooth)
{
  nav_smooth->priv = hyscan_nav_smooth_get_instance_private (nav_smooth);
}

static void
hyscan_nav_smooth_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanNavSmooth *nav_smooth = HYSCAN_NAV_SMOOTH (object);
  HyScanNavSmoothPrivate *priv = nav_smooth->priv;

  switch (prop_id)
    {
    case PROP_NAV_DATA:
      priv->nav_data = g_value_dup_object (value);
      break;

    case PROP_CIRCULAR:
      priv->circular = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_nav_smooth_object_constructed (GObject *object)
{
  HyScanNavSmooth *nav_smooth = HYSCAN_NAV_SMOOTH (object);

  G_OBJECT_CLASS (hyscan_nav_smooth_parent_class)->constructed (object);
}

static void
hyscan_nav_smooth_object_finalize (GObject *object)
{
  HyScanNavSmooth *nav_smooth = HYSCAN_NAV_SMOOTH (object);
  HyScanNavSmoothPrivate *priv = nav_smooth->priv;

  g_object_unref (priv->nav_data);

  G_OBJECT_CLASS (hyscan_nav_smooth_parent_class)->finalize (object);
}

/* Находит средневзвешенное значений lvalue и rvalue. */
static inline gdouble
hyscan_nav_smooth_weight (gint64  time,
                          gint64  ltime,
                          gint64  rtime,
                          gdouble lvalue,
                          gdouble rvalue)
{
  gint64 dtime;
  gdouble rweight, lweight;

  dtime = rtime - ltime;

  if (dtime == 0)
    return lvalue;

  rweight = 1.0 - (gdouble) (rtime - time) / dtime;
  lweight = 1.0 - (gdouble) (time - ltime) / dtime;

  return lweight * lvalue + rweight * rvalue;
}

/* Находит круговое средневзвешенное значений lvalue и rvalue. */
static inline gdouble
hyscan_nav_smooth_weight_circular (gint64  time,
                                   gint64  ltime,
                                   gint64  rtime,
                                   gdouble lvalue,
                                   gdouble rvalue)
{
  gint64 dtime;
  gdouble rweight, lweight;
  gdouble sum_sin, sum_cos;
  gdouble value;

  dtime = rtime - ltime;

  if (dtime == 0)
    return lvalue;

  rweight = 1.0 - (gdouble) (rtime - time) / dtime;
  lweight = 1.0 - (gdouble) (time - ltime) / dtime;

  rvalue *= G_PI / 180.0;
  lvalue *= G_PI / 180.0;

  sum_sin = rweight * sin (rvalue) + lweight * sin (lvalue);
  sum_cos = rweight * cos (rvalue) + lweight * cos (lvalue);
  value = atan2 (sum_sin, sum_cos) / G_PI * 180.0;

  if (value < 0.)
    value += 360.0;

  if (value == 360.0)
    value = 0.0;

  return value;
}

/**
 * hyscan_nav_smooth_new_circular:
 * @nav_data: источник навигационных данных
 *
 * Функция создает объект для интерполяции навигационных данных.
 *
 * Returns: (transfer full): указатель на #HyScanNavData, для удаления g_object_unref().
 */
HyScanNavSmooth *
hyscan_nav_smooth_new (HyScanNavData *nav_data)
{
  return g_object_new (HYSCAN_TYPE_NAV_SMOOTH,
                       "nav-data", nav_data,
                       NULL);
}

/**
 * hyscan_nav_smooth_new_circular:
 * @nav_data: источник угловых навигационных данных, определённых в градусах
 *
 * Функция создает объект для интерполяции круговых величин со значениями
 * от 0 до 360 градусов.
 *
 * Returns: (transfer full): указатель на #HyScanNavData, для удаления g_object_unref().
 */
HyScanNavSmooth *
hyscan_nav_smooth_new_circular (HyScanNavData *nav_data)
{
  return g_object_new (HYSCAN_TYPE_NAV_SMOOTH,
                       "nav-data", nav_data,
                       "circular", TRUE,
                       NULL);
}

/**
 * hyscan_nav_smooth_get:
 * @nav_smooth: указатель на #HyScanNavSmooth
 * @cancellable: (nullable): #HyScanCancellable
 * @time: время, мкс
 * @value: (out): значение
 *
 * Функция определяет значение величины в момент времени @time с помощью интерполяции
 * данных из используемого источника данных.
 *
 * Returns: %TRUE, если значение было определено.
 */
gboolean
hyscan_nav_smooth_get (HyScanNavSmooth   *nav_smooth,
                       HyScanCancellable *cancellable,
                       gint64             time,
                       gdouble           *value)
{
  HyScanNavSmoothPrivate *priv;
  guint32 lindex, rindex;
  gint64 ltime, rtime;
  gdouble lvalue, rvalue;
  gboolean found;
  
  g_return_val_if_fail (HYSCAN_IS_NAV_SMOOTH (nav_smooth), FALSE);
  priv = nav_smooth->priv;

  HyScanDBFindStatus find_status;

  find_status = hyscan_nav_data_find_data (priv->nav_data, time, &lindex, &rindex, &ltime, &rtime);
  if (find_status != HYSCAN_DB_FIND_OK)
    return FALSE;

  // todo: если в текущем индексе нет данных, то пробовать брать соседние
  found = hyscan_nav_data_get (priv->nav_data, cancellable, lindex, NULL, &lvalue) &&
          hyscan_nav_data_get (priv->nav_data, cancellable, rindex, NULL, &rvalue);

  if (!found)
    return FALSE;

  if (priv->circular)
    *value = hyscan_nav_smooth_weight_circular (time, ltime, rtime, lvalue, rvalue);
  else
    *value = hyscan_nav_smooth_weight (time, ltime, rtime, lvalue, rvalue);

  return TRUE;
}

/**
 * hyscan_nav_smooth_get_data:
 * @nav_smooth: указатель на #HyScanNavSmooth
 *
 * Получает указатель на связанный с @nav_smooth объект навигационных данных.
 *
 * Returns: (transfer none): указатель на #HyScanNavData
 */
HyScanNavData *
hyscan_nav_smooth_get_data (HyScanNavSmooth *nav_smooth)
{
  g_return_val_if_fail (HYSCAN_IS_NAV_SMOOTH (nav_smooth), NULL);

  return nav_smooth->priv->nav_data;
}

/* hyscan-planner-model.c
 *
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
 * SECTION: hyscan-planner-model
 * @Short_description: Модель данных планировщика
 * @Title: HyScanPlannerModel
 *
 * #HyScanPlannerModel предназначен для асинхронной работы с объектами планировщика.
 * Класс модели расширяет функциональность класса #HyScanObjectModel и позволяет
 * устанавливать географические координаты начала отсчёта топоцентрической системы
 * координат и получить объект пересчёта координат из географической СК
 * в топоцентрическую.
 *
 */

#include "hyscan-planner-model.h"

struct _HyScanPlannerModelPrivate
{
  HyScanGeo                   *geo;
};

static void    hyscan_planner_model_object_finalize    (GObject               *object);
static void    hyscan_planner_model_changed            (HyScanObjectModel     *model);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlannerModel, hyscan_planner_model, HYSCAN_TYPE_OBJECT_MODEL)

static void
hyscan_planner_model_class_init (HyScanPlannerModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectModelClass *model_class = HYSCAN_OBJECT_MODEL_CLASS (klass);

  model_class->changed = hyscan_planner_model_changed;
  object_class->finalize = hyscan_planner_model_object_finalize;
}

static void
hyscan_planner_model_init (HyScanPlannerModel *planner_model)
{
  planner_model->priv = hyscan_planner_model_get_instance_private (planner_model);
}

static void
hyscan_planner_model_object_finalize (GObject *object)
{
  HyScanPlannerModel *planner_model = HYSCAN_PLANNER_MODEL (object);
  HyScanPlannerModelPrivate *priv = planner_model->priv;

  g_clear_object (&priv->geo);

  G_OBJECT_CLASS (hyscan_planner_model_parent_class)->finalize (object);
}

static void
hyscan_planner_model_changed (HyScanObjectModel *model)
{
  HyScanPlannerModel *pmodel = HYSCAN_PLANNER_MODEL (model);
  HyScanPlannerModelPrivate *priv = pmodel->priv;
  HyScanPlannerOrigin *origin;

  g_clear_object (&priv->geo);

  origin = hyscan_object_model_get_id (model, HYSCAN_PLANNER_ORIGIN_ID);
  if (origin == NULL)
    return;

  priv->geo = hyscan_geo_new (origin->origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  hyscan_planner_origin_free (origin);
}

/**
 * hyscan_planner_model_new:
 *
 * Создаёт модель данных для асинхронного доступа к параметрам объектов планировщика.
 *
 * Returns: (transfer-full): указатель на HyScanPlannerModel. Для удаления g_object_unref().
 */
HyScanPlannerModel *
hyscan_planner_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_PLANNER_MODEL,
                       "data-type", HYSCAN_TYPE_PLANNER_DATA, NULL);
}

/**
 * hyscan_planner_model_get_geo:
 * @pmodel: указатель на #HyScanPlannerModel
 *
 * Функция возвращает объект пересчёта координат схемы галсов из географической
 * системы координат в топоцентрическую и обратно.
 *
 * Returns: (transfer-full): (nullable): указатель на объект HyScanGeo, для удаления
 *   g_object_unref().
 */
HyScanGeo *
hyscan_planner_model_get_geo (HyScanPlannerModel *pmodel)
{
  HyScanPlannerModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel), NULL);
  priv = pmodel->priv;

  return priv->geo != NULL ? g_object_ref (priv->geo) : NULL;
}

/**
 * hyscan_planner_model_set_origin:
 * @pmodel: указатель на #HyScanPlannerModel
 * @origin: координаты начала отсчёта топоцентрической системы координат
 *
 * Функция устанавливает начало топоцентрической системы координат. Направление
 * оси OX указывается в поле .h структуры @origin в градусах.
 *
 * Фактическое изменение объекта пересчёта координат произойдёт после записи
 * данных в БД, т.е. при одном из следующих испусканий сигналов "changed".
 */
void
hyscan_planner_model_set_origin (HyScanPlannerModel      *pmodel,
                                 const HyScanGeoGeodetic *origin)
{
  HyScanObjectModel *model;
  HyScanPlannerOrigin *prev_value;

  g_return_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel));
  model = HYSCAN_OBJECT_MODEL (pmodel);

  prev_value = hyscan_planner_model_get_origin (pmodel);

  if (origin == NULL)
    {
      if (prev_value != NULL)
        hyscan_object_model_remove_object (model, HYSCAN_PLANNER_ORIGIN_ID);
    }
  else
    {
      HyScanPlannerOrigin ref_point;

      ref_point.type = HYSCAN_PLANNER_ORIGIN;
      ref_point.origin = *origin;

      if (prev_value != NULL)
        hyscan_object_model_modify_object (model, HYSCAN_PLANNER_ORIGIN_ID, &ref_point);
      else
        hyscan_object_model_add_object (model, &ref_point);
    }

  g_clear_pointer (&prev_value, hyscan_planner_origin_free);
}

/**
 * hyscan_planner_model_get_origin:
 * @pmodel: указатель на #HyScanPlannerModel
 *
 * Возвращает координаты точки отсчёта для схемы галсов проекта.
 *
 * Returns: (transfer-full): указатель на структуру #HyScanPlannerOrigin.
 *   Для удаления hyscan_planner_origin_free
 */
HyScanPlannerOrigin *
hyscan_planner_model_get_origin (HyScanPlannerModel *pmodel)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel), NULL);

  return hyscan_object_model_get_id (HYSCAN_OBJECT_MODEL (pmodel), HYSCAN_PLANNER_ORIGIN_ID);
}

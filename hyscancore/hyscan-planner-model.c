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

HyScanPlannerModel *
hyscan_planner_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_PLANNER_MODEL,
                       "data-type", HYSCAN_TYPE_PLANNER_DATA, NULL);
}

HyScanGeo *
hyscan_planner_model_get_geo (HyScanPlannerModel *pmodel)
{
  HyScanPlannerModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel), NULL);
  priv = pmodel->priv;

  return priv->geo != NULL ? g_object_ref (priv->geo) : NULL;
}

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

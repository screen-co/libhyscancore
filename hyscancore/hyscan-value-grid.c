#include "hyscan-value-grid.h"
#include "hyscan-cartesian.h"

#define INDEX_FROM_IJ(i, j) ((i) + priv->size * (j))

enum
{
  PROP_O,
  PROP_START,
  PROP_STEP,
  PROP_SIZE,
};

struct _HyScanValueGridPrivate
{
  GHashTable           *values;
  HyScanGeoCartesian2D  start;
  HyScanGeoCartesian2D  end;
  gint                  size;
  gdouble               step;
};

static void    hyscan_value_grid_set_property          (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_value_grid_object_constructed    (GObject               *object);
static void    hyscan_value_grid_object_finalize       (GObject               *object);
static void    hyscan_value_grid_array_free            (GArray                *array);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanValueGrid, hyscan_value_grid, G_TYPE_OBJECT)

static void
hyscan_value_grid_class_init (HyScanValueGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_value_grid_set_property;

  object_class->constructed = hyscan_value_grid_object_constructed;
  object_class->finalize = hyscan_value_grid_object_finalize;

  g_object_class_install_property (object_class, PROP_START,
    g_param_spec_pointer ("start", "Start", "Start point",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_STEP,
    g_param_spec_double ("step", "Step", "Step size", 0, G_MAXDOUBLE, 1.0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SIZE,
    g_param_spec_uint ("size", "Size", "Number of cells on grid size", 0, G_MAXUINT, 255,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_value_grid_init (HyScanValueGrid *value_grid)
{
  value_grid->priv = hyscan_value_grid_get_instance_private (value_grid);
}

static void
hyscan_value_grid_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanValueGrid *value_grid = HYSCAN_VALUE_GRID (object);
  HyScanValueGridPrivate *priv = value_grid->priv;

  switch (prop_id)
    {
    case PROP_START:
      if (g_value_get_pointer (value) != NULL)
        priv->start = * (HyScanGeoCartesian2D *) g_value_get_pointer (value);
      break;

    case PROP_SIZE:
      priv->size = g_value_get_uint (value);
      break;

    case PROP_STEP:
      priv->step = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
hyscan_value_grid_object_constructed (GObject *object)
{
  HyScanValueGrid *value_grid = HYSCAN_VALUE_GRID (object);
  HyScanValueGridPrivate *priv = value_grid->priv;

  G_OBJECT_CLASS (hyscan_value_grid_parent_class)->constructed (object);

  priv->end.x = priv->start.x + priv->size * priv->step;
  priv->end.y = priv->start.y + priv->size * priv->step;
  priv->values = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) hyscan_value_grid_array_free);
}

static void
hyscan_value_grid_object_finalize (GObject *object)
{
  HyScanValueGrid *value_grid = HYSCAN_VALUE_GRID (object);
  HyScanValueGridPrivate *priv = value_grid->priv;

  g_hash_table_unref (priv->values);

  G_OBJECT_CLASS (hyscan_value_grid_parent_class)->finalize (object);
}

static void
hyscan_value_grid_array_free (GArray *array)
{
  g_array_free (array, TRUE);
}

static gint
hyscan_value_grid_xy2int (HyScanValueGrid      *value_grid,
                          HyScanGeoCartesian2D *point)
{
  HyScanValueGridPrivate *priv = value_grid->priv;
  gint i, j;

  gboolean ok = priv->start.x < point->x && point->x < priv->end.x &&
                priv->start.y < point->y && point->y < priv->end.y;

  if (!ok)
    return -1;

  i = (point->x - priv->start.x) / priv->step;
  j = (point->y - priv->start.y) / priv->step;

  return INDEX_FROM_IJ (i, j);
}

static void
hyscan_value_grid_int2xy (HyScanValueGrid      *value_grid,
                          gint                  index,
                          HyScanGeoCartesian2D *point)
{
  HyScanValueGridPrivate *priv = value_grid->priv;
  gint i, j;

  j = index / priv->size;
  i = index - j;

  point->x = i * priv->step + priv->start.x;
  point->y = j * priv->step + priv->start.y;
}

HyScanValueGrid *
hyscan_value_grid_new (HyScanGeoCartesian2D    start,
                       gdouble                 step,
                       guint                   size)
{
  return g_object_new (HYSCAN_TYPE_VALUE_GRID,
                       "start", &start,
                       "step", step,
                       "size", size,
                       NULL);
}

gboolean
hyscan_value_grid_get (HyScanValueGrid        *value_grid,
                       HyScanGeoCartesian2D   *point,
                       gdouble                *value)
{
  HyScanValueGridPrivate *priv;
  GArray *values;
  gint index;
  gdouble aggregate;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_VALUE_GRID (value_grid), FALSE);

  priv = value_grid->priv;

  index = hyscan_value_grid_xy2int (value_grid, point);
  if (index < 0)
    return FALSE;

  values = g_hash_table_lookup (priv->values, GINT_TO_POINTER (index));
  if (values == NULL)
    return FALSE;

  aggregate = 0;
  for (i = 0; i < values->len; i++)
    aggregate += g_array_index (values, gdouble, i);

  *value = aggregate / values->len;

  return TRUE;
}

gboolean
hyscan_value_grid_get_index (HyScanValueGrid      *value_grid,
                             gint                  i,
                             gint                  j,
                             gdouble              *value)
{
  HyScanValueGridPrivate *priv;
  GArray *values;
  gint index;
  gdouble aggregate;
  guint k;

  g_return_val_if_fail (HYSCAN_IS_VALUE_GRID (value_grid), FALSE);

  priv = value_grid->priv;

  index = INDEX_FROM_IJ (i, j);
  values = g_hash_table_lookup (priv->values, GINT_TO_POINTER (index));
  if (values == NULL)
    return FALSE;

  aggregate = 0;
  for (k = 0; k < values->len; k++)
    aggregate += g_array_index (values, gdouble, k);

  *value = aggregate / values->len;

  return TRUE;
}

static GArray *
hyscan_value_grid_cell (HyScanValueGridPrivate *priv,
                        gint                    index)
{
  GArray *values;

  values = g_hash_table_lookup (priv->values, GINT_TO_POINTER (index));
  if (values != NULL)
    return values;

  values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  g_hash_table_insert (priv->values, GINT_TO_POINTER (index), values);

  return values;
}

gboolean
hyscan_value_grid_add (HyScanValueGrid      *value_grid,
                       HyScanGeoCartesian2D *point,
                       gdouble               value)
{
  HyScanValueGridPrivate *priv;
  gint index;
  GArray *values;

  g_return_val_if_fail (HYSCAN_IS_VALUE_GRID (value_grid), FALSE);

  priv = value_grid->priv;

  index = hyscan_value_grid_xy2int (value_grid, point);
  if (index < 0)
    return FALSE;

  values = hyscan_value_grid_cell (priv, index);

  g_array_append_val (values, value);
  return TRUE;
}

/* Функция проверяет, находится ли полигон внутри ячейки (i, j). */
static gboolean
hyscan_value_grid_inside (HyScanValueGridPrivate *priv,
                          gint                    i,
                          gint                    j,
                          HyScanGeoCartesian2D  *vertices,
                          gsize                  vertices_len)
{
  gdouble step = priv->step;
  HyScanGeoCartesian2D a = { priv->start.x + i * step, priv->start.y + j * step };
  HyScanGeoCartesian2D b = { a.x, a.y + step };
  HyScanGeoCartesian2D c = {a.x + step, a.y + step };
  HyScanGeoCartesian2D d = {a.x + step, a.y };

  gsize k, m;

  /* Полигон находится внутри ячейки (точнее, хотя бы одна его вершина). */
  if (hyscan_cartesian_is_point_inside (&vertices[0], &a, &c))
    return TRUE;

  /* Ячейка находится внутри полигона (точнее, хотя бы одна её вершина). */
  if (hyscan_cartesian_is_inside_polygon (vertices, vertices_len, &a))
    return TRUE;

  /* Проверяем пересечение полигона с ячейкой. */
  for (k = 0; k < vertices_len; k++)
    {
      gboolean intersect;

      m = (k + 1) % vertices_len;
      intersect = hyscan_cartesian_segments_intersect (&a, &b, &vertices[k], &vertices[m]) ||
                  hyscan_cartesian_segments_intersect (&b, &c, &vertices[k], &vertices[m]) ||
                  hyscan_cartesian_segments_intersect (&c, &d, &vertices[k], &vertices[m]) ||
                  hyscan_cartesian_segments_intersect (&d, &a, &vertices[k], &vertices[m]);

      if (intersect)
        return TRUE;
    }

  return FALSE;
}

void
hyscan_value_grid_area (HyScanValueGrid        *value_grid,
                        HyScanGeoCartesian2D   *vertices,
                        gsize                   vertices_len,
                        gdouble                 value)
{
  HyScanValueGridPrivate *priv;
  guint k;
  gint i, j;
  gint i_first, j_first, i_last, j_last;
  gdouble min_x, min_y, max_x, max_y;

  g_return_if_fail (HYSCAN_IS_VALUE_GRID (value_grid));

  priv = value_grid->priv;

  /* Границы сетки. */
  min_x = priv->end.x;
  min_y = priv->end.y;
  max_x = priv->start.x;
  max_y = priv->start.y;

  /* Находим границы полигона. */
  for (k = 0; k < vertices_len; k++)
    {
      min_x = MIN (min_x, vertices[k].x);
      min_y = MIN (min_y, vertices[k].y);
      max_x = MAX (max_x, vertices[k].x);
      max_y = MAX (max_y, vertices[k].y);
    }

  /* Полигон не попадает на сетку. */
  if (min_x > max_x || min_y > max_y)
    return;

  i_first = (min_x - priv->start.x) / priv->step;
  j_first = (min_y - priv->start.y) / priv->step;
  i_last = (max_x - priv->start.x) / priv->step;
  j_last = (max_y - priv->start.y) / priv->step;

  i_first = CLAMP (i_first, 0, priv->size);
  j_first = CLAMP (j_first, 0, priv->size);
  i_last = CLAMP (i_last, 0, priv->size);
  j_last = CLAMP (j_last, 0, priv->size);
  /* Проходим по всем ячейкам внутри найденной зоны. */
  for (i = i_first; i <= i_last; i++)
    {
      for (j = j_first; j <= j_last; j++)
        {
          GArray *array;

          if (!hyscan_value_grid_inside (priv, i, j, vertices, vertices_len))
            continue;

          array = hyscan_value_grid_cell (priv, INDEX_FROM_IJ (i, j));
          g_array_append_val (array, value);
        }
    }
}

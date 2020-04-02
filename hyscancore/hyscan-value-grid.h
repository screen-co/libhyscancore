#ifndef __HYSCAN_VALUE_GRID_H__
#define __HYSCAN_VALUE_GRID_H__

#include <glib-object.h>
#include <hyscan-geo.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_VALUE_GRID             (hyscan_value_grid_get_type ())
#define HYSCAN_VALUE_GRID(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_VALUE_GRID, HyScanValueGrid))
#define HYSCAN_IS_VALUE_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_VALUE_GRID))
#define HYSCAN_VALUE_GRID_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_VALUE_GRID, HyScanValueGridClass))
#define HYSCAN_IS_VALUE_GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_VALUE_GRID))
#define HYSCAN_VALUE_GRID_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_VALUE_GRID, HyScanValueGridClass))

typedef struct _HyScanValueGrid HyScanValueGrid;
typedef struct _HyScanValueGridPrivate HyScanValueGridPrivate;
typedef struct _HyScanValueGridClass HyScanValueGridClass;

struct _HyScanValueGrid
{
  GObject parent_instance;

  HyScanValueGridPrivate *priv;
};

struct _HyScanValueGridClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_value_grid_get_type         (void);

HYSCAN_API
HyScanValueGrid *      hyscan_value_grid_new              (HyScanGeoCartesian2D    start,
                                                           gdouble                 step,
                                                           guint                   size);
HYSCAN_API
gboolean               hyscan_value_grid_get              (HyScanValueGrid        *value_grid,
                                                           HyScanGeoCartesian2D   *point,
                                                           gdouble                *value);
HYSCAN_API
gboolean               hyscan_value_grid_get_index        (HyScanValueGrid        *value_grid,
                                                           gint                    i,
                                                           gint                    j,
                                                           gdouble                *value);
HYSCAN_API
gboolean               hyscan_value_grid_add              (HyScanValueGrid        *value_grid,
                                                           HyScanGeoCartesian2D   *point,
                                                           gdouble                 value);
HYSCAN_API
void                   hyscan_value_grid_area             (HyScanValueGrid        *value_grid,
                                                           HyScanGeoCartesian2D   *vertices,
                                                           gsize                   vertices_len,
                                                           gdouble                 value);

G_END_DECLS

#endif /* __HYSCAN_VALUE_GRID_H__ */

#ifndef __HYSCAN_NAV_STATE_H__
#define __HYSCAN_NAV_STATE_H__

#include <glib-object.h>
#include "hyscan-geo.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAV_STATE            (hyscan_nav_state_get_type ())
#define HYSCAN_NAV_STATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_STATE, HyScanNavState))
#define HYSCAN_IS_NAV_STATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_STATE))
#define HYSCAN_NAV_STATE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_NAV_STATE, HyScanNavStateInterface))

typedef struct _HyScanNavState HyScanNavState;
typedef struct _HyScanNavStateInterface HyScanNavStateInterface;
typedef struct _HyScanNavStateData HyScanNavStateData;

/**
 * HyScanNavStateData:
 * @loaded: признак того, что навигационные данные присутствуют
 * @time: текущее время, сек
 * @coord: положение объекта: широта и долгота
 * @speed: скорость движения, м/с
 * @cog: путевой угол (COG), радианы
 * @true_heading: признак того, что указан истинный курс (HDT)
 * @heading: истинный курс, если @true_heading = %TRUE, иначе путевой угол, радианы
 *
 * Навигационные данные
 */
struct _HyScanNavStateData
{
  gboolean            loaded;

  gdouble             time;
  HyScanGeoPoint      coord;
  gdouble             cog;
  gboolean            true_heading;
  gdouble             heading;
  gdouble             speed;
};

struct _HyScanNavStateInterface
{
  GTypeInterface      g_iface;

  gboolean            (*get)                           (HyScanNavState      *state,
                                                        HyScanNavStateData  *data,
                                                        gdouble             *time_delta);
};

HYSCAN_API
GType                 hyscan_nav_state_get_type        (void);

HYSCAN_API
gboolean              hyscan_nav_state_get             (HyScanNavState      *state,
                                                        HyScanNavStateData  *data,
                                                        gdouble             *time_delta);

G_END_DECLS

#endif /* __HYSCAN_NAV_STATE_H__ */

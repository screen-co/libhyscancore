#ifndef __HYSCAN_NAV_SIMPLE_H__
#define __HYSCAN_NAV_SIMPLE_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAV_SIMPLE             (hyscan_nav_simple_get_type ())
#define HYSCAN_NAV_SIMPLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_SIMPLE, HyScanNavSimple))
#define HYSCAN_IS_NAV_SIMPLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_SIMPLE))
#define HYSCAN_NAV_SIMPLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NAV_SIMPLE, HyScanNavSimpleClass))
#define HYSCAN_IS_NAV_SIMPLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NAV_SIMPLE))
#define HYSCAN_NAV_SIMPLE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NAV_SIMPLE, HyScanNavSimpleClass))

typedef struct _HyScanNavSimple HyScanNavSimple;
typedef struct _HyScanNavSimplePrivate HyScanNavSimplePrivate;
typedef struct _HyScanNavSimpleClass HyScanNavSimpleClass;

struct _HyScanNavSimple
{
  GObject parent_instance;

  HyScanNavSimplePrivate *priv;
};

struct _HyScanNavSimpleClass
{
  GObjectClass parent_class;
};

GType hyscan_nav_simple_get_type (void);

G_END_DECLS

#endif /* __HYSCAN_NAV_SIMPLE_H__ */

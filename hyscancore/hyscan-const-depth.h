#ifndef __HYSCAN_CONST_DEPTH_H__
#define __HYSCAN_CONST_DEPTH_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CONST_DEPTH             (hyscan_const_depth_get_type ())
#define HYSCAN_CONST_DEPTH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CONST_DEPTH, HyScanConstDepth))
#define HYSCAN_IS_CONST_DEPTH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CONST_DEPTH))
#define HYSCAN_CONST_DEPTH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CONST_DEPTH, HyScanConstDepthClass))
#define HYSCAN_IS_CONST_DEPTH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CONST_DEPTH))
#define HYSCAN_CONST_DEPTH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CONST_DEPTH, HyScanConstDepthClass))

typedef struct _HyScanConstDepth HyScanConstDepth;
typedef struct _HyScanConstDepthPrivate HyScanConstDepthPrivate;
typedef struct _HyScanConstDepthClass HyScanConstDepthClass;

/* !!! Change GObject to type of the base class. !!! */
struct _HyScanConstDepth
{
  GObject parent_instance;

  HyScanConstDepthPrivate *priv;
};

/* !!! Change GObjectClass to type of the base class. !!! */
struct _HyScanConstDepthClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType hyscan_const_depth_get_type (void);

HYSCAN_API
HyScanConstDepth*       hyscan_const_depth_new           (HyScanDB         *db,
                                                          const gchar      *project,
                                                          const gchar      *track,
                                                          HyScanSourceType  source_type,
                                                          gboolean          raw);

HYSCAN_API
void                    hyscan_const_depth_set_distance  (HyScanConstDepth *depth,
                                                          gfloat            distance);

G_END_DECLS

#endif /* __HYSCAN_CONST_DEPTH_H__ */

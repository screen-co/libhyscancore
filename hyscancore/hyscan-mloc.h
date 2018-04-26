#ifndef __HYSCAN_MLOC_H__
#define __HYSCAN_MLOC_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-geo.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MLOC             (hyscan_mloc_get_type ())
#define HYSCAN_MLOC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MLOC, HyScanmLoc))
#define HYSCAN_IS_MLOC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MLOC))
#define HYSCAN_MLOC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MLOC, HyScanmLocClass))
#define HYSCAN_IS_MLOC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MLOC))
#define HYSCAN_MLOC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MLOC, HyScanmLocClass))

typedef struct _HyScanmLoc HyScanmLoc;
typedef struct _HyScanmLocPrivate HyScanmLocPrivate;
typedef struct _HyScanmLocClass HyScanmLocClass;

struct _HyScanmLoc
{
  GObject parent_instance;

  HyScanmLocPrivate *priv;
};

struct _HyScanmLocClass
{
  GObjectClass parent_class;
};

GType                   hyscan_mloc_get_type         (void);

HyScanmLoc             *hyscan_mloc_new              (HyScanDB       *db,
                                                      const gchar    *project,
                                                      const gchar    *track);

gboolean                hyscan_mloc_get              (HyScanmLoc            *mloc,
                                                      gint64                 time,
                                                      HyScanAntennaPosition *antenna,
                                                      gdouble                shift,
                                                      HyScanGeoGeodetic     *position);
G_END_DECLS

#endif /* __HYSCAN_MLOC_H__ */

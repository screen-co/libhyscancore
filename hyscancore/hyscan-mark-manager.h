#ifndef __HYSCAN_MARK_MANAGER_H__
#define __HYSCAN_MARK_MANAGER_H__

#include <hyscan-waterfall-mark-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MANAGER             (hyscan_mark_manager_get_type ())
#define HYSCAN_MARK_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManager))
#define HYSCAN_IS_MARK_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MANAGER))
#define HYSCAN_MARK_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManagerClass))
#define HYSCAN_IS_MARK_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MANAGER))
#define HYSCAN_MARK_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManagerClass))

typedef struct _HyScanMarkManager HyScanMarkManager;
typedef struct _HyScanMarkManagerPrivate HyScanMarkManagerPrivate;
typedef struct _HyScanMarkManagerClass HyScanMarkManagerClass;

struct _HyScanMarkManager
{
  GObject parent_instance;

  HyScanMarkManagerPrivate *priv;
};

struct _HyScanMarkManagerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_mark_manager_get_type      (void);

HYSCAN_API
HyScanMarkManager      *hyscan_mark_manager_new           (HyScanDB            *db);

HYSCAN_API
void                    hyscan_mark_manager_set_profile   (HyScanMarkManager     *model,
                                                         const gchar         *profile);
HYSCAN_API
void                    hyscan_mark_manager_set_track     (HyScanMarkManager     *model,
                                                         const gchar         *project,
                                                         const gchar         *track);

HYSCAN_API
void                    hyscan_mark_manager_add           (HyScanMarkManager     *model,
                                                         HyScanWaterfallMark *mark);

HYSCAN_API
void                    hyscan_mark_manager_modify        (HyScanMarkManager     *model,
                                                         const gchar         *id,
                                                         HyScanWaterfallMark *mark);

HYSCAN_API
void                    hyscan_mark_manager_remove        (HyScanMarkManager     *model,
                                                         const gchar         *id);

HYSCAN_API
GHashTable             *hyscan_mark_manager_get           (HyScanMarkManager     *model);

G_END_DECLS

#endif /* __HYSCAN_MARK_MANAGER_H__ */

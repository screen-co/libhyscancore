#ifndef __HYSCAN_PROFILE_HW_H__
#define __HYSCAN_PROFILE_HW_H__

#include <hyscan-control.h>
#include "hyscan-profile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_HW             (hyscan_profile_hw_get_type ())
#define HYSCAN_PROFILE_HW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_HW, HyScanProfileHW))
#define HYSCAN_IS_PROFILE_HW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_HW))
#define HYSCAN_PROFILE_HW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_HW, HyScanProfileHWClass))
#define HYSCAN_IS_PROFILE_HW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_HW))
#define HYSCAN_PROFILE_HW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_HW, HyScanProfileHWClass))

typedef struct _HyScanProfileHW HyScanProfileHW;
typedef struct _HyScanProfileHWPrivate HyScanProfileHWPrivate;
typedef struct _HyScanProfileHWClass HyScanProfileHWClass;

struct _HyScanProfileHW
{
  HyScanProfile parent_instance;

  HyScanProfileHWPrivate *priv;
};

struct _HyScanProfileHWClass
{
  HyScanProfileClass parent_class;
};

HYSCAN_API
GType                  hyscan_profile_hw_get_type         (void);

HYSCAN_API
HyScanProfileHW *      hyscan_profile_hw_new              (const gchar     *file);

HYSCAN_API
void                   hyscan_profile_hw_set_driver_paths (HyScanProfileHW *profile,
                                                           gchar          **driver_paths);

HYSCAN_API
GList *                hyscan_profile_hw_list             (HyScanProfileHW *profile);

HYSCAN_API
gboolean               hyscan_profile_hw_check            (HyScanProfileHW *profile);

HYSCAN_API
HyScanControl *        hyscan_profile_hw_connect          (HyScanProfileHW *profile);

HYSCAN_API
HyScanControl *        hyscan_profile_hw_connect_simple   (const gchar     *file);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_HW_H__ */

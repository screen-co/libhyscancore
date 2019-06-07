#ifndef __HYSCAN_PROFILE_OFFSET_H__
#define __HYSCAN_PROFILE_OFFSET_H__

#include <hyscan-api.h>
#include <hyscan-types.h>
#include <hyscan-control.h>
#include "hyscan-profile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_OFFSET             (hyscan_profile_offset_get_type ())
#define HYSCAN_PROFILE_OFFSET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_OFFSET, HyScanProfileOffset))
#define HYSCAN_IS_PROFILE_OFFSET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_OFFSET))
#define HYSCAN_PROFILE_OFFSET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_OFFSET, HyScanProfileOffsetClass))
#define HYSCAN_IS_PROFILE_OFFSET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_OFFSET))
#define HYSCAN_PROFILE_OFFSET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_OFFSET, HyScanProfileOffsetClass))

typedef struct _HyScanProfileOffset HyScanProfileOffset;
typedef struct _HyScanProfileOffsetPrivate HyScanProfileOffsetPrivate;
typedef struct _HyScanProfileOffsetClass HyScanProfileOffsetClass;

struct _HyScanProfileOffset
{
  HyScanProfile parent_instance;

  HyScanProfileOffsetPrivate *priv;
};

struct _HyScanProfileOffsetClass
{
  HyScanProfileClass parent_class;
};

HYSCAN_API
GType                  hyscan_profile_offset_get_type         (void);

HYSCAN_API
HyScanProfileOffset *  hyscan_profile_offset_new              (const gchar         *file);

HYSCAN_API
GHashTable *           hyscan_profile_offset_get_sonars       (HyScanProfileOffset *profile);

HYSCAN_API
GHashTable *           hyscan_profile_offset_get_sensors      (HyScanProfileOffset *profile);

HYSCAN_API
gboolean               hyscan_profile_offset_apply            (HyScanProfileOffset *profile,
                                                               HyScanControl       *control);
HYSCAN_API
gboolean               hyscan_profile_offset_apply_default    (HyScanProfileOffset *profile,
                                                               HyScanControl       *control);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_OFFSET_H__ */

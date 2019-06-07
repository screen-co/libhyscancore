// TODO: this naming sucks
#ifndef __HYSCAN_PROFILE_HW_DEVICE_H__
#define __HYSCAN_PROFILE_HW_DEVICE_H__

#include <hyscan-param-list.h>
#include <hyscan-discover.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_HW_DEVICE             (hyscan_profile_hw_device_get_type ())
#define HYSCAN_PROFILE_HW_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_HW_DEVICE, HyScanProfileHWDevice))
#define HYSCAN_IS_PROFILE_HW_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_HW_DEVICE))
#define HYSCAN_PROFILE_HW_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_HW_DEVICE, HyScanProfileHWDeviceClass))
#define HYSCAN_IS_PROFILE_HW_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_HW_DEVICE))
#define HYSCAN_PROFILE_HW_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_HW_DEVICE, HyScanProfileHWDeviceClass))

typedef struct _HyScanProfileHWDevice HyScanProfileHWDevice;
typedef struct _HyScanProfileHWDevicePrivate HyScanProfileHWDevicePrivate;
typedef struct _HyScanProfileHWDeviceClass HyScanProfileHWDeviceClass;

/* TODO: Change GObject to type of the base class. */
struct _HyScanProfileHWDevice
{
  GObject parent_instance;

  HyScanProfileHWDevicePrivate *priv;
};

/* TODO: Change GObjectClass to type of the base class. */
struct _HyScanProfileHWDeviceClass
{
  GObjectClass parent_class;
};

GType                   hyscan_profile_hw_device_get_type         (void);

HyScanProfileHWDevice * hyscan_profile_hw_device_new              (GKeyFile              *keyfile);
void                    hyscan_profile_hw_device_set_group        (HyScanProfileHWDevice *hw_device,
                                                                   const gchar           *group);
void                    hyscan_profile_hw_device_set_paths        (HyScanProfileHWDevice *hw_device,
                                                                   gchar                **paths);
void                    hyscan_profile_hw_device_read             (HyScanProfileHWDevice *hw_device,
                                                                   GKeyFile              *kf);
gboolean                hyscan_profile_hw_device_check            (HyScanProfileHWDevice *hw_device);
HyScanDevice *          hyscan_profile_hw_device_connect          (HyScanProfileHWDevice *hw_device);

//gchar *                 hyscan_profile_hw_device_get_uri          (HyScanProfileHWDevice *hw_device);
//gchar *                 hyscan_profile_hw_device_get_driver       (HyScanProfileHWDevice *hw_device);
//HyScanParamList *       hyscan_profile_hw_device_get_params       (HyScanProfileHWDevice *hw_device,
//                                                                   HyScanDataSchema      *schema);

// void                    hyscan_profile_hw_device_set_uri          (HyScanProfileHWDevice *hw_device,
//                                                                    gchar                 *uri);
// void                    hyscan_profile_hw_device_set_driver       (HyScanProfileHWDevice *hw_device,
//                                                                    gchar                 *driver);
// void                    hyscan_profile_hw_device_set_params       (HyScanProfileHWDevice *hw_device,
//                                                                    HyScanParamList       *params);

// void                    hyscan_profile_hw_device_write            (HyScanProfileHWDevice *hw_device);
// void                    hyscan_profile_hw_device_read             (HyScanProfileHWDevice *hw_device);



G_END_DECLS

#endif /* __HYSCAN_PROFILE_HW_DEVICE_H__ */

#ifndef __HYSCAN_AMPLITUDE_SIMPLE_H__
#define __HYSCAN_AMPLITUDE_SIMPLE_H__

#include <hyscan-acoustic-data.h>
#include <hyscan-amplitude.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_AMPLITUDE_SIMPLE             (hyscan_amplitude_simple_get_type ())
#define HYSCAN_AMPLITUDE_SIMPLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_AMPLITUDE_SIMPLE, HyScanAmplitudeSimple))
#define HYSCAN_IS_AMPLITUDE_SIMPLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_AMPLITUDE_SIMPLE))
#define HYSCAN_AMPLITUDE_SIMPLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_AMPLITUDE_SIMPLE, HyScanAmplitudeSimpleClass))
#define HYSCAN_IS_AMPLITUDE_SIMPLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_AMPLITUDE_SIMPLE))
#define HYSCAN_AMPLITUDE_SIMPLE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_AMPLITUDE_SIMPLE, HyScanAmplitudeSimpleClass))

typedef struct _HyScanAmplitudeSimple HyScanAmplitudeSimple;
typedef struct _HyScanAmplitudeSimplePrivate HyScanAmplitudeSimplePrivate;
typedef struct _HyScanAmplitudeSimpleClass HyScanAmplitudeSimpleClass;

/* TODO: Change HyScanAcousticData to type of the base class. */
struct _HyScanAmplitudeSimple
{
  HyScanAcousticData parent_instance;

  HyScanAmplitudeSimplePrivate *priv;
};

/* TODO: Change HyScanAcousticDataClass to type of the base class. */
struct _HyScanAmplitudeSimpleClass
{
  HyScanAcousticDataClass parent_class;
};

GType                   hyscan_amplitude_simple_get_type         (void);

HyScanAmplitudeSimple * hyscan_amplitude_simple_new              (HyScanDB              *db,
                                                                  HyScanCache           *cache,
                                                                  const gchar           *project_name,
                                                                  const gchar           *track_name,
                                                                  HyScanSourceType       source,
                                                                  guint                  channel,
                                                                  gboolean               noise);

G_END_DECLS

#endif /* __HYSCAN_AMPLITUDE_SIMPLE_H__ */

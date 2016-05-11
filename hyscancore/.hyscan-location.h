#ifndef __HYSCAN_LOCATION_H__
#define __HYSCAN_LOCATION_H__

#include <glib-object.h>
#include <hyscan-types.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_LOCATION            (hyscan_location_get_type ())
#define HYSCAN_LOCATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_LOCATION, HyScanLocation))
#define HYSCAN_IS_LOCATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_LOCATION))
#define HYSCAN_LOCATION_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_LOCATION, HyScanLocationInterface))

/** \brief Элемент таблицы профиля скорости звука */
typedef struct
{
  gint64 time;
  gdouble depth;      /**< Глубина в м. */
  gdouble soundspeed; /**< Скорость звука в м/с. */
}SoundSpeedTable;

typedef struct
{
  gint64 time;
  gdouble depth;        /**< . */
  gdouble q;            /**< . */
} HyScanDepthData;

typedef struct
{
  gint64 time;
  gdouble latitude;     /**< . */
  gdouble longitude;    /**< . */
  gdouble q;
} HyScanLatLongData;

typedef struct
{
  gint64 time;
  gdouble altitude;     /**< . */
  gdouble q;
} HyScanAltitudeData;

typedef struct
{
  gint64 time;
  gdouble speed;        /**< . */
  gdouble q;
} HyScanSpeedData;

typedef struct
{
  gint64 time;
  gdouble track;        /**< . */
  gdouble q;
} HyScanTrackData;

typedef struct
{
  gint64 time;
  gdouble roll;         /**< . */
  gdouble pitch;        /**< . */
  gdouble q;
} HyScanRollPitchData;

typedef struct _HyScanLocation HyScanLocation;
typedef struct _HyScanLocationInterface HyScanLocationInterface;

struct _HyScanLocationInterface
{
  GTypeInterface       g_iface;

  void                  (*set_soundspeed)       (HyScanLocation *location,
                                                 GArray         *soundspeedtable);
  HyScanDepthData       (*get_depth)            (HyScanLocation *location,
                                                 gint32          index);
  HyScanLatLongData     (*get_latlong)          (HyScanLocation *location,
                                                 gint32          index);
  HyScanAltitudeData    (*get_altitude)         (HyScanLocation *location,
                                                 gint32          index);
  HyScanSpeedData       (*get_speed)            (HyScanLocation *location,
                                                 gint32          index);
  HyScanTrackData       (*get_track)            (HyScanLocation *location,
                                                 gint32          index);
  HyScanRollPitchData   (*get_rollpitch)        (HyScanLocation *location,
                                                 gint32          index);


};

GType hyscan_location_get_type                  (void);

/**
 *
 * Функция позволяет задать профиль скорости звука
 *
 * \param depth указатель на интерфейс \link HyScanLocation \endlink, c которым в данный момент ведется работа;
 * \param soundspeedtable указатель на GArray, состоящий из элементов типа SoundSpeedTable
 *
 */
HYSCAN_CORE_EXPORT
void                    hyscan_location_set_soundspeed  (HyScanLocation *depth,
                                                         GArray         *soundspeedtable);

HYSCAN_CORE_EXPORT
HyScanDepthData         hyscan_location_get_depth       (HyScanLocation *location,
                                                         gint32          index);

HYSCAN_CORE_EXPORT
HyScanLatLongData       hyscan_location_get_latlong     (HyScanLocation *location,
                                                         gint32          index);

HYSCAN_CORE_EXPORT
HyScanAltitudeData      hyscan_location_get_altitude    (HyScanLocation *location,
                                                         gint32          index);

HYSCAN_CORE_EXPORT
HyScanSpeedData         hyscan_location_get_speed       (HyScanLocation *location,
                                                         gint32          index);

HYSCAN_CORE_EXPORT
HyScanTrackData         hyscan_location_get_track       (HyScanLocation *location,
                                                         gint32          index);
HYSCAN_CORE_EXPORT
HyScanRollPitchData     hyscan_location_get_rollpitch   (HyScanLocation *location,
                                                         gint32          index);

G_END_DECLS

#endif /* __HYSCAN_LOCATION_H__ */

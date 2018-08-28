/* hyscan-acoustic-data.h
 *
 * Copyright 2015-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

#ifndef __HYSCAN_ACOUSTIC_DATA_H__
#define __HYSCAN_ACOUSTIC_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_ACOUSTIC_DATA             (hyscan_acoustic_data_get_type ())
#define HYSCAN_ACOUSTIC_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_ACOUSTIC_DATA, HyScanAcousticData))
#define HYSCAN_IS_ACOUSTIC_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_ACOUSTIC_DATA))
#define HYSCAN_ACOUSTIC_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_ACOUSTIC_DATA, HyScanAcousticDataClass))
#define HYSCAN_IS_ACOUSTIC_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_ACOUSTIC_DATA))
#define HYSCAN_ACOUSTIC_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_ACOUSTIC_DATA, HyScanAcousticDataClass))

typedef struct _HyScanAcousticData HyScanAcousticData;
typedef struct _HyScanAcousticDataPrivate HyScanAcousticDataPrivate;
typedef struct _HyScanAcousticDataClass HyScanAcousticDataClass;

struct _HyScanAcousticData
{
  GObject parent_instance;

  HyScanAcousticDataPrivate *priv;
};

struct _HyScanAcousticDataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_acoustic_data_get_type           (void);

HYSCAN_API
HyScanAcousticData            *hyscan_acoustic_data_new                (HyScanDB              *db,
                                                                        HyScanCache           *cache,
                                                                        const gchar           *project_name,
                                                                        const gchar           *track_name,
                                                                        HyScanSourceType       source,
                                                                        guint                  channel,
                                                                        gboolean               noise);

HYSCAN_API
HyScanDB *                     hyscan_acoustic_data_get_db             (HyScanAcousticData    *data);

HYSCAN_API
const gchar *                  hyscan_acoustic_data_get_project_name   (HyScanAcousticData    *data);

HYSCAN_API
const gchar *                  hyscan_acoustic_data_get_track_name     (HyScanAcousticData    *data);

HYSCAN_API
HyScanSourceType               hyscan_acoustic_data_get_source         (HyScanAcousticData    *data);

HYSCAN_API
guint                          hyscan_acoustic_data_get_channel        (HyScanAcousticData    *data);

HYSCAN_API
gboolean                       hyscan_acoustic_data_is_noise           (HyScanAcousticData    *data);

HYSCAN_API
HyScanDiscretizationType       hyscan_acoustic_data_get_discretization (HyScanAcousticData    *data);

HYSCAN_API
HyScanAntennaPosition          hyscan_acoustic_data_get_position       (HyScanAcousticData    *data);

HYSCAN_API
HyScanAcousticDataInfo         hyscan_acoustic_data_get_info           (HyScanAcousticData    *data);

HYSCAN_API
gboolean                       hyscan_acoustic_data_is_writable        (HyScanAcousticData    *data);

HYSCAN_API
gboolean                       hyscan_acoustic_data_has_tvg            (HyScanAcousticData    *data);

HYSCAN_API
guint32                        hyscan_acoustic_data_get_mod_count      (HyScanAcousticData    *data);

HYSCAN_API
gboolean                       hyscan_acoustic_data_get_range          (HyScanAcousticData    *data,
                                                                        guint32               *first_index,
                                                                        guint32               *last_index);

HYSCAN_API
HyScanDBFindStatus             hyscan_acoustic_data_find_data          (HyScanAcousticData    *data,
                                                                        gint64                 time,
                                                                        guint32               *lindex,
                                                                        guint32               *rindex,
                                                                        gint64                *ltime,
                                                                        gint64                *rtime);

HYSCAN_API
void                           hyscan_acoustic_data_set_convolve       (HyScanAcousticData    *data,
                                                                        gboolean               convolve,
                                                                        gdouble                scale);

HYSCAN_API
gboolean                       hyscan_acoustic_data_get_size_time      (HyScanAcousticData    *data,
                                                                        guint32                index,
                                                                        guint32               *n_points,
                                                                        gint64                *time);

HYSCAN_API
const HyScanComplexFloat *     hyscan_acoustic_data_get_signal         (HyScanAcousticData    *data,
                                                                        guint32                index,
                                                                        guint32               *n_points,
                                                                        gint64                *time);

HYSCAN_API
const gfloat *                 hyscan_acoustic_data_get_tvg            (HyScanAcousticData    *data,
                                                                        guint32                index,
                                                                        guint32               *n_points,
                                                                        gint64                *time);

HYSCAN_API
const gfloat *                 hyscan_acoustic_data_get_real           (HyScanAcousticData    *data,
                                                                        guint32                index,
                                                                        guint32               *n_points,
                                                                        gint64                *time);

HYSCAN_API
const HyScanComplexFloat *     hyscan_acoustic_data_get_complex        (HyScanAcousticData    *data,
                                                                        guint32                index,
                                                                        guint32               *n_points,
                                                                        gint64                *time);

HYSCAN_API
const gfloat *                 hyscan_acoustic_data_get_amplitude      (HyScanAcousticData    *data,
                                                                        guint32                index,
                                                                        guint32               *n_points,
                                                                        gint64                *time);

G_END_DECLS

#endif /* __HYSCAN_ACOUSTIC_DATA_H__ */

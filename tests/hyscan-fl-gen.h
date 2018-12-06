/* hyscan-fl-gen.h
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_FL_GEN_H__
#define __HYSCAN_FL_GEN_H__

#include <hyscan-forward-look-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_FL_GEN             (hyscan_fl_gen_get_type ())
#define HYSCAN_FL_GEN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_FL_GEN, HyScanFLGen))
#define HYSCAN_IS_FL_GEN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_FL_GEN))
#define HYSCAN_FL_GEN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_FL_GEN, HyScanFLGenClass))
#define HYSCAN_IS_FL_GEN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_FL_GEN))
#define HYSCAN_FL_GEN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_FL_GEN, HyScanFLGenClass))

typedef struct _HyScanFLGen HyScanFLGen;
typedef struct _HyScanFLGenPrivate HyScanFLGenPrivate;
typedef struct _HyScanFLGenClass HyScanFLGenClass;

struct _HyScanFLGen
{
  GObject parent_instance;

  HyScanFLGenPrivate *priv;
};

struct _HyScanFLGenClass
{
  GObjectClass parent_class;
};

GType                  hyscan_fl_gen_get_type          (void);

HyScanFLGen           *hyscan_fl_gen_new               (void);

void                   hyscan_fl_gen_set_position      (HyScanFLGen                   *fl_gen,
                                                        HyScanAntennaPosition         *position);

void                   hyscan_fl_gen_set_info          (HyScanFLGen                   *fl_gen,
                                                        HyScanAcousticDataInfo        *info);

gboolean               hyscan_fl_gen_set_track         (HyScanFLGen                   *fl_gen,
                                                        HyScanDB                      *db,
                                                        const gchar                   *project_name,
                                                        const gchar                   *track_name);

gboolean               hyscan_fl_gen_generate          (HyScanFLGen                   *fl_gen,
                                                        guint32                        n_points,
                                                        gint64                         time);

gboolean               hyscan_fl_gen_check             (const HyScanForwardLookDOA    *doa,
                                                        guint32                        n_points,
                                                        gint64                         time,
                                                        gdouble                        alpha);


G_END_DECLS

#endif /* __HYSCAN_FL_GEN_H__ */

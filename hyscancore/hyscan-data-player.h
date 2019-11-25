/* hyscan-data-player.h
 *
 * Copyright 2018-2019 Screen LLC, Bugrov Sergey <bugrov@screen-co.ru>
 * Copyright 2019 Screen LLC, Maxim Sidorenko <sidormax@mail.ru>
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

#ifndef __HYSCAN_DATA_PLAYER_H__
#define __HYSCAN_DATA_PLAYER_H__

#include <hyscan-db.h>
#include <hyscan-types.h>
#include <hyscan-core-common.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DATA_PLAYER             (hyscan_data_player_get_type ())
#define HYSCAN_DATA_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DATA_PLAYER, HyScanDataPlayer))
#define HYSCAN_IS_DATA_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DATA_PLAYER))
#define HYSCAN_DATA_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DATA_PLAYER, HyScanDataPlayerClass))
#define HYSCAN_IS_DATA_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DATA_PLAYER))
#define HYSCAN_DATA_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DATA_PLAYER, HyScanDataPlayerClass))

typedef struct _HyScanDataPlayer HyScanDataPlayer;
typedef struct _HyScanDataPlayerPrivate HyScanDataPlayerPrivate;
typedef struct _HyScanDataPlayerClass HyScanDataPlayerClass;

struct _HyScanDataPlayer
{
  GObject parent_instance;

  HyScanDataPlayerPrivate *priv;
};

struct _HyScanDataPlayerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType              hyscan_data_player_get_type         (void);

HYSCAN_API
HyScanDataPlayer * hyscan_data_player_new              (void);

HYSCAN_API
void               hyscan_data_player_shutdown         (HyScanDataPlayer *player);

HYSCAN_API
void               hyscan_data_player_set_track        (HyScanDataPlayer *player,
                                                        HyScanDB         *db,
                                                        const gchar      *project_name,
                                                        const gchar      *track_name);

HYSCAN_API
void               hyscan_data_player_set_fps          (HyScanDataPlayer *player,
                                                        guint32           fps);

HYSCAN_API
HyScanDB *         hyscan_data_player_get_db           (HyScanDataPlayer *player);

HYSCAN_API
const gchar *      hyscan_data_player_get_project_name (HyScanDataPlayer *player);

HYSCAN_API
const gchar *      hyscan_data_player_get_track_name   (HyScanDataPlayer *player);

HYSCAN_API
gboolean           hyscan_data_player_is_played        (HyScanDataPlayer *player);

HYSCAN_API
gint32             hyscan_data_player_add_channel      (HyScanDataPlayer *player,
                                                        HyScanSourceType  source,
                                                        guint             channel,
                                                        HyScanChannelType type);

HYSCAN_API
gboolean           hyscan_data_player_remove_channel   (HyScanDataPlayer *player,
                                                        gint32            id);

HYSCAN_API
void               hyscan_data_player_clear_channels   (HyScanDataPlayer *player);

HYSCAN_API
gboolean           hyscan_data_player_channel_is_exist (HyScanDataPlayer *player,
                                                        gint32            id);

HYSCAN_API
void               hyscan_data_player_play             (HyScanDataPlayer *player,
                                                        gdouble           speed);

HYSCAN_API
void               hyscan_data_player_pause            (HyScanDataPlayer *player);

HYSCAN_API
void               hyscan_data_player_stop             (HyScanDataPlayer *player);

HYSCAN_API
void               hyscan_data_player_real_time        (HyScanDataPlayer *player);

HYSCAN_API
void               hyscan_data_player_seek             (HyScanDataPlayer *player,
                                                        gint64            time);

HYSCAN_API
void               hyscan_data_player_seek_next        (HyScanDataPlayer *player);

HYSCAN_API
void               hyscan_data_player_seek_prev        (HyScanDataPlayer *player);

HYSCAN_API
void               hyscan_data_player_step             (HyScanDataPlayer *player,
                                                        gint32            steps);

#endif /* __HYSCAN_DATA_PLAYER_H__ */

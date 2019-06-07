/**
 * \file hyscan-profile-db.h
 *
 * \brief Заголовочный файл класса HyScanProfileDB - профиля БД.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanProfileDB HyScanProfileDB - профиль БД.
 *
 */

#ifndef __HYSCAN_PROFILE_DB_H__
#define __HYSCAN_PROFILE_DB_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-db.h>
#include "hyscan-profile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_DB            (hyscan_profile_db_get_type ())
#define HYSCAN_PROFILE_DB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_DB, HyScanProfileDB))
#define HYSCAN_IS_PROFILE_DB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_DB))
#define HYSCAN_PROFILE_DB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_DB, HyScanProfileDBClass))
#define HYSCAN_IS_PROFILE_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_DB))
#define HYSCAN_PROFILE_DB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_DB, HyScanProfileDBClass))

typedef struct _HyScanProfileDB HyScanProfileDB;
typedef struct _HyScanProfileDBPrivate HyScanProfileDBPrivate;
typedef struct _HyScanProfileDBClass HyScanProfileDBClass;

struct _HyScanProfileDB
{
  HyScanProfile             parent_instance;
  HyScanProfileDBPrivate   *priv;
};

struct _HyScanProfileDBClass
{
  HyScanProfileClass   parent_class;
};

HYSCAN_API
GType              hyscan_profile_db_get_type            (void);

/* Создаёт объект HyScanProfileDB. */
HYSCAN_API
HyScanProfileDB*   hyscan_profile_db_new                 (const gchar       *file);

/* Получает имя системы хранения. */
HYSCAN_API
const gchar*       hyscan_profile_db_get_name            (HyScanProfileDB   *profile);

/* Получает URI системы хранения. */
HYSCAN_API
const gchar*       hyscan_profile_db_get_uri             (HyScanProfileDB   *profile);

/* Задаёт имя системы хранения. */
HYSCAN_API
void               hyscan_profile_db_set_name            (HyScanProfileDB   *profile,
                                                          const gchar       *name);

/* Задаёт URI системы хранения. */
HYSCAN_API
void               hyscan_profile_db_set_uri             (HyScanProfileDB   *profile,
                                                          const gchar       *uri);

HYSCAN_API
HyScanDB *         hyscan_profile_db_connect             (HyScanProfileDB   *profile);


G_END_DECLS

#endif /* __HYSCAN_PROFILE_DB_H__ */

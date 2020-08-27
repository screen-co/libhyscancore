#ifndef __HYSCAN_OBJECT_STORE_H__
#define __HYSCAN_OBJECT_STORE_H__

#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_STORE            (hyscan_object_store_get_type ())
#define HYSCAN_OBJECT_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_STORE, HyScanObjectStore))
#define HYSCAN_IS_OBJECT_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_STORE))
#define HYSCAN_OBJECT_STORE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_OBJECT_STORE, HyScanObjectStoreInterface))

typedef struct _HyScanObjectStore HyScanObjectStore;
typedef struct _HyScanObjectStoreInterface HyScanObjectStoreInterface;
typedef struct _HyScanObject HyScanObject;

/**
 * HyScanObject:
 * @type: тип GBoxed
 *
 * Все структуры, которые загружаются при помощи #HyScanObjectData, должны быть
 * зарегистрированы как типы GBoxed, а в поле type хранить идентификатор своего типа GType.
 * При передаче структуры в функции hyscan_object_data_add(), hyscan_object_data_modify()
 * и подобные, структура должна быть приведена к типу #HyScanObject.
 */
struct _HyScanObject
{
  GType                type;
};

struct _HyScanObjectStoreInterface
{
  GTypeInterface       g_iface;

  HyScanObject *       (*get)                                  (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

  gchar **             (*get_ids)                              (HyScanObjectStore  *store,
                                                                GType               type);

  GHashTable *         (*get_all)                              (HyScanObjectStore  *store,
                                                                GType               type);

  gboolean             (*add)                                  (HyScanObjectStore  *store,
                                                                const HyScanObject *object,
                                                                gchar             **given_id);

  gboolean             (*modify)                               (HyScanObjectStore  *store,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

  gboolean             (*set)                                  (HyScanObjectStore  *store,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

  gboolean             (*remove)                               (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

  guint32              (*get_mod_count)                        (HyScanObjectStore  *store,
                                                                GType               type);
};

HYSCAN_API
GType                  hyscan_object_store_get_type            (void);

HYSCAN_API
HyScanObject *         hyscan_object_store_get                 (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

HYSCAN_API
gchar **               hyscan_object_store_get_ids             (HyScanObjectStore  *store,
                                                                GType               type);

HYSCAN_API
GHashTable *           hyscan_object_store_get_all             (HyScanObjectStore  *store,
                                                                GType               type);

HYSCAN_API
gboolean               hyscan_object_store_add                 (HyScanObjectStore  *store,
                                                                const HyScanObject *object,
                                                                gchar             **given_id);

HYSCAN_API
gboolean               hyscan_object_store_modify              (HyScanObjectStore  *store,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

HYSCAN_API
gboolean               hyscan_object_store_set                 (HyScanObjectStore  *store,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

HYSCAN_API
gboolean               hyscan_object_store_remove              (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

HYSCAN_API
guint32                hyscan_object_store_get_mod_count       (HyScanObjectStore  *store,
                                                                GType               type);

HYSCAN_API
HyScanObject *         hyscan_object_copy                      (const HyScanObject *object);

HYSCAN_API
void                   hyscan_object_free                      (HyScanObject       *object);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_STORE_H__ */

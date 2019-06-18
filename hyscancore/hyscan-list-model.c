/* hyscan-list-model.с
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

/**
 * SECTION: hyscan-list-model
 * @Short_description: Модель списка активных элементов
 * @Title: HyScanListModel
 *
 * Модель хранит список строк (например, идентификаторы активных элементов)
 * и оповещает своих подписчиков о любых изменения в этом списке.
 *
 * - hyscan_list_model_new() - создание модели;
 * - hyscan_list_model_get() - получение таблицы активных элементов;
 * - hyscan_list_model_add() - добавление элемента в список;
 * - hyscan_list_model_remove() - удаление элемента из списка;
 * - hyscan_list_model_has() - получение статуса элемента.
 *
 * Для подписки на события изменения используется сигнал #HyScanListModel::changed.
 *
 * Класс не является потокобезопасным.
 *
 */

#include "hyscan-list-model.h"

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

struct _HyScanListModelPrivate
{
  GHashTable *table;
};

static void    hyscan_list_model_object_constructed       (GObject               *object);
static void    hyscan_list_model_object_finalize          (GObject               *object);

static guint   hyscan_list_model_signals[SIGNAL_LAST] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (HyScanListModel, hyscan_list_model, G_TYPE_OBJECT)

static void
hyscan_list_model_class_init (HyScanListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_list_model_object_constructed;
  object_class->finalize = hyscan_list_model_object_finalize;

  /**
   * HyScanListModel::changed:
   * @list_model: указатель на #HyScanListModel
   *
   * Сигнал посылается при изменении списка активных элементов.
   */
  hyscan_list_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_LIST_MODEL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_list_model_init (HyScanListModel *list_model)
{
  list_model->priv = hyscan_list_model_get_instance_private (list_model);
}

static void
hyscan_list_model_object_constructed (GObject *object)
{
  HyScanListModel *list_model = HYSCAN_LIST_MODEL (object);
  HyScanListModelPrivate *priv = list_model->priv;

  G_OBJECT_CLASS (hyscan_list_model_parent_class)->constructed (object);

  priv->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
hyscan_list_model_object_finalize (GObject *object)
{
  HyScanListModel *list_model = HYSCAN_LIST_MODEL (object);
  HyScanListModelPrivate *priv = list_model->priv;

  g_hash_table_unref (priv->table);

  G_OBJECT_CLASS (hyscan_list_model_parent_class)->finalize (object);
}

/**
 * hyscan_list_model_new:
 *
 * Returns: новый объект #HyScanListModel
 */
HyScanListModel *
hyscan_list_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_LIST_MODEL, NULL);
}

/**
 * hyscan_list_model_get:
 * @list_model: указатель на #HyScanListModel
 *
 * Returns: NULL-терминированный массив элементов списка. Для удаления g_strfreev()
 */
gchar **
hyscan_list_model_get (HyScanListModel *list_model)
{
  HyScanListModelPrivate *priv;
  gchar ** keys;
  gchar **list;

  g_return_val_if_fail (HYSCAN_IS_LIST_MODEL (list_model), NULL);
  priv = list_model->priv;

  keys = (gchar **) g_hash_table_get_keys_as_array (priv->table, NULL);
  list = g_strdupv (keys);

  g_free (keys);

  return list;
}

/**
 * hyscan_list_model_add:
 * @list_model: указатель на #HyScanListModel
 * @key: имя элемента
 * @active: признак активности элемента
 *
 * Устанавливает элемент с именем @key активным или неактивным.
 */
void
hyscan_list_model_add (HyScanListModel *list_model,
                       const gchar     *key)
{
  HyScanListModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_LIST_MODEL (list_model));
  priv = list_model->priv;

  g_hash_table_insert (priv->table, g_strdup (key), GINT_TO_POINTER (TRUE));

  g_signal_emit (list_model, hyscan_list_model_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_list_model_remove:
 * @list_model: указатель на #HyScanListModel
 * @key: имя элемента
 * @active: признак активности элемента
 *
 * Добавляет элемент @key в список.
 */
void
hyscan_list_model_remove (HyScanListModel *list_model,
                          const gchar     *key)
{
  HyScanListModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_LIST_MODEL (list_model));
  priv = list_model->priv;

  g_hash_table_remove (priv->table, key);

  g_signal_emit (list_model, hyscan_list_model_signals[SIGNAL_CHANGED], 0);
}

void
hyscan_list_model_remove_all (HyScanListModel *list_model)
{
  HyScanListModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_LIST_MODEL (list_model));
  priv = list_model->priv;

  g_hash_table_remove_all (priv->table);

  g_signal_emit (list_model, hyscan_list_model_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_list_model_has:
 * @list_model: указатель на #HyScanListModel
 * @key: имя элемента
 *
 * Returns: возвращает %TRUE, если @key есть в списке; иначе %FALSE.
 */
gboolean
hyscan_list_model_has (HyScanListModel *list_model,
                       const gchar          *key)
{
  HyScanListModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_LIST_MODEL (list_model), FALSE);
  priv = list_model->priv;

  return GPOINTER_TO_INT (g_hash_table_lookup (priv->table, key));
}

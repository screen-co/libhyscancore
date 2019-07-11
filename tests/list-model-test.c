/* list-model-test.c
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

#include <hyscan-list-model.h>

static gint test_count;

static void
test_one_item (HyScanListModel *model,
               gpointer         user_data)
{
  const gchar *item_name = user_data;
  gchar **items;
  
  g_assert (hyscan_list_model_has (model, "item1") == g_str_equal (item_name, "item1"));
  g_assert (hyscan_list_model_has (model, "item2") == g_str_equal (item_name, "item2"));
  
  items = hyscan_list_model_get (model);
  g_assert_cmpint (g_strv_length (items), ==, 1);
  g_assert_cmpstr (items[0], ==, item_name);
  g_strfreev (items);

  ++test_count;
}

static void
test_two_items (HyScanListModel *model,
                gpointer         user_data)
{
  gchar **items;
  
  g_assert_true (hyscan_list_model_has (model, "item1"));
  g_assert_true (hyscan_list_model_has (model, "item2"));
  
  items = hyscan_list_model_get (model);
  g_assert_cmpint (g_strv_length (items), ==, 2);
  g_strfreev (items);

  ++test_count;
}

static void
test_no_items (HyScanListModel *model,
               gpointer         user_data)
{
  gchar **items;

  g_assert_false (hyscan_list_model_has (model, "item1"));
  g_assert_false (hyscan_list_model_has (model, "item2"));

  items = hyscan_list_model_get (model);
  g_assert_cmpint (g_strv_length (items), ==, 0);
  g_strfreev (items);

  ++test_count;
}

void
replace_signal_handler (HyScanListModel *list,
                        GCallback        callback,
                        gpointer         user_data)
{
  static gulong handler_id;

  if (handler_id != 0)
    g_signal_handler_disconnect (list, handler_id);

  if (callback == NULL)
    return;

  handler_id = g_signal_connect (list, "changed", G_CALLBACK (callback), user_data);
  g_assert (handler_id != 0);
}

int 
main (int    argc,
      char **argv)
{
  HyScanListModel *list;
  
  list = hyscan_list_model_new ();
  
  /* 1. Добавляем один элемент. */
  replace_signal_handler (list, G_CALLBACK (test_one_item), "item1");
  hyscan_list_model_add (list, "item1");
  
  /* 2. Добавляем второй элемент. */
  replace_signal_handler (list, G_CALLBACK (test_two_items), NULL);
  hyscan_list_model_add (list, "item2");

  /* 3. Добавляем уже существующий элемент. */
  replace_signal_handler (list, G_CALLBACK (test_two_items), NULL);
  hyscan_list_model_add (list, "item2");

  /* 4. Удаляем первый элемент. */
  replace_signal_handler (list, G_CALLBACK (test_one_item), "item2");
  hyscan_list_model_remove (list, "item1");

  /* 5. Удаляем все. */
  replace_signal_handler (list, G_CALLBACK (test_no_items), NULL);
  hyscan_list_model_remove_all (list);

  g_assert_cmpint (test_count, ==, 5);

  g_object_unref (list);

  g_message ("Test done!");
  
  return 0;
}

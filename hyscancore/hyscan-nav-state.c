/* hyscan-nav-state.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-nav-state
 * @Short_description: модель навигационных данных
 * @Title: HyScanNavState
 *
 * Интерфейс предназначен для получения текущей информации о местоположении и
 * курсе движения судна.
 *
 * При изменении состояния интерфейс эмитирует сигнал #HyScanNavState::nav-changed
 * с информацией о текущем местоположении и времени его фиксации.
 *
 */
#include "hyscan-nav-state.h"

G_DEFINE_INTERFACE (HyScanNavState, hyscan_nav_state, G_TYPE_OBJECT)

static void
hyscan_nav_state_default_init (HyScanNavStateInterface *iface)
{
  /**
   * HyScanNavState::nav-changed:
   * @model: указатель на #HyScanNavModel
   * @data: указатель на навигационные данные #HyScanNavModelData
   *
   * Сигнал сообщает об изменении текущего местоположения.
   */
  g_signal_new ("nav-changed", HYSCAN_TYPE_NAV_STATE,
                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE,
                1, G_TYPE_POINTER);
}

/**
 * hyscan_nav_state_get:
 * @model: указатель на #HyScanNavState
 * @data: (out): данные модели #HyScanNavModelData
 * @time_delta: (out) (nullable): возраст данных @data в секундах
 *
 * Записывает текущие данные модели в @data. Возраст @time_delta показывает
 * время в секундах, прошедшее с того момента, когда данные @data были
 * актуальными.
 *
 * Returns: %TRUE, если данные получены успешно.
 */
gboolean
hyscan_nav_state_get (HyScanNavState      *nav_state,
                      HyScanNavStateData  *data,
                      gdouble             *time_delta)
{
  HyScanNavStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_STATE (nav_state), FALSE);

  iface = HYSCAN_NAV_STATE_GET_IFACE (nav_state);
  if (iface->get != NULL)
    return (* iface->get) (nav_state, data, time_delta);

  return FALSE;
}

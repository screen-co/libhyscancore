/* hyscan-mother-factory.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanCore.
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
 * SECTION: hyscan-mother-factory
 * @Title HyScanMotherFactory
 * @Short_description
 *
 */

#include "hyscan-mother-factory.h"

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

static guint hyscan_mother_factory_signals[SIGNAL_LAST] = {0};
G_DEFINE_TYPE (HyScanMotherFactory, hyscan_mother_factory, G_TYPE_OBJECT);

static void
hyscan_mother_factory_class_init (HyScanMotherFactoryClass *klass)
{
  hyscan_mother_factory_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_MOTHER_FACTORY,
                  G_SIGNAL_ACTION, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_mother_factory_init (HyScanMotherFactory *mother)
{

}

void
hyscan_mother_factory_emit_changed (HyScanMotherFactory *mother)
{
  g_return_if_fail (HYSCAN_IS_MOTHER_FACTORY (mother));
  g_signal_emit (mother, hyscan_mother_factory_signals[SIGNAL_CHANGED], 0);
}

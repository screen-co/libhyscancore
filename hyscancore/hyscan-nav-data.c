/* hyscan-nav-data.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-nav-data
 * @Short_description: HyScanNavData - интерфейс для получения навигационных данных
 * @Title: HyScanNavData
 *
 * Интерфейс #HyScanNavData предназначен для объектов,
 * реализующих доступ к навигационным данным: широте, долготе, курсу, глубине и так далее.
 * Под элементарными в данном случае подразумеваются все те классы, которые не производят
 * сглаживание и усреднение данных, а только обрабатывают строки данных и выдают
 * одно и только одно значение. Иными словами, эти классы оперируют индексами,
 * в то время как более высокоуровневые классы оперируют временем. Все классы,
 * реализующие этот интерфейс являются надстройками над БД, возвращающими не целые строки
 * данных, а обработанные.
 *
 * Для того, чтобы более высокоуровневые классы могли отличать данные, полученные
 * из разных источников (но одним интерфейсом), вводится понятие токена. Токен - это
 * строка, однозначно определяющая внутреннее состояние класса. У двух объектов,
 * обрабатывающих один и тот же навигационный параметр с одинаковыми параметрами
 * токен должен быть одинаковым. Например, курс можно извлекать из RMC-строк или
 * вычислять из координат. В этом случае токен должен различаться.
 */

#include "hyscan-nav-data.h"

G_DEFINE_INTERFACE (HyScanNavData, hyscan_nav_data, G_TYPE_OBJECT);

static void
hyscan_nav_data_default_init (HyScanNavDataInterface *iface)
{
}

/**
 * hyscan_nav_data_get:
 * @ndata: указатель на интерфейс #HyScanNavData
 * @cancellable: #HyScanCancellable
 * @index: индекс записи в канале данных
 * @time: (out) (nullable): время данных
 * @value: (out) (nullable): значение
 *
 * Функция возвращает значение для заданного индекса.
 *
 * Returns: %TRUE, если удалось определить, %FALSE в случае ошибки или если
 * cancellable был отменен.
 */
gboolean
hyscan_nav_data_get (HyScanNavData     *navdata,
                     HyScanCancellable *cancellable,
                     guint32            index,
                     gint64            *time,
                     gdouble           *value)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), FALSE);

  if (g_cancellable_is_cancelled (G_CANCELLABLE (cancellable)))
    return FALSE;

  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get != NULL)
    return (*iface->get) (navdata, cancellable, index, time, value);

  return FALSE;
}

/**
 * hyscan_nav_data_find_data:
 * @ndata: указатель на интерфейс #HyScanNavData
 * @time: искомый момент времени
 * @lindex: (out) (nullable): указатель на переменную для сохранения "левого" индекса
 * @rindex: (out) (nullable): указатель на переменную для сохранения "правого" индекса
 * @ltime: (out) (nullable): указатель на переменную для сохранения "левой" метки времени
 * @rtime: (out) (nullable): указатель на переменную для сохранения "правой" метки времени
 *
 * Функция ищет индекс данных для указанного момента времени. Функция возвращает статус поиска
 * индекса данных #HyScanDBFindStatus.
 *
 * Если найдены данные, метка времени которых точно совпадает с указанной, значения lindex и rindex,
 * а также ltime и rtime будут одинаковые.
 *
 * Если найдены данные, метка времени которых находится в переделах записанных данных, значения
 * lindex и ltime будут указывать на индекс и время соответственно, данных, записанных перед искомым
 * моментом времени. А rindex и ltime будут указывать на индекс и время соответственно, данных, записанных
 * после искомого момента времени.
 *
 * Returns: Статус поиска индекса данных.
 */
HyScanDBFindStatus
hyscan_nav_data_find_data (HyScanNavData *navdata,
                           gint64         time,
                           guint32       *lindex,
                           guint32       *rindex,
                           gint64        *ltime,
                           gint64        *rtime)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), HYSCAN_DB_FIND_FAIL);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->find_data != NULL)
    return (*iface->find_data) (navdata, time, lindex, rindex, ltime, rtime);

  return HYSCAN_DB_FIND_FAIL;
}

/**
 * hyscan_nav_data_get_range:
 * @ndata: указатель на интерфейс #HyScanNavData
 * @first: (out) (nullable): указатель на переменную для сохранения самого раннего индекса данных
 * @last: (out) (nullable): указатель на переменную для сохранения самого позднего индекса данных
 *
 * Функция возвращает диапазон данных в используемом канале данных.
 *
 * Returns: %TRUE - если границы записей определены, %FALSE - в случае ошибки.
 */
gboolean
hyscan_nav_data_get_range (HyScanNavData *navdata,
                           guint32       *first,
                           guint32       *last)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), FALSE);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_range != NULL)
    return (*iface->get_range) (navdata, first, last);

  return FALSE;
}

/**
 * hyscan_nav_data_get_offset:
 * @ndata: указатель на интерфейс #HyScanNavData
 *
 * Функция возвращает информацию о смещении приёмной антенны
 * в виде значения структуры #HyScanAntennaOffset.
 *
 * Returns: Смещение приёмной антенны.
 */
HyScanAntennaOffset
hyscan_nav_data_get_offset (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;
  HyScanAntennaOffset zero = {0};

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), zero);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_offset != NULL)
    return (*iface->get_offset) (navdata);

  return zero;
}

/**
 * hyscan_nav_data_is_writable:
 * @ndata: указатель на интерфейс #HyScanNavData
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * Returns: %TRUE - если возможна запись данных, %FALSE - если данные в режиме только чтения.
 */
gboolean
hyscan_nav_data_is_writable (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), FALSE);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->is_writable != NULL)
    return (*iface->is_writable) (navdata);

  return FALSE;
}

/**
 * hyscan_nav_data_get_token:
 * @ndata: указатель на интерфейс #HyScanNavData
 *
 * Функция возвращает строку, содержащую идентификатор внутренних параметров класса,
 * реализующего интерфейс.
 *
 * Одни данные глубины могут быть получены из разных источников: NMEA, ГБО, эхолот и т.д.
 * Возможно сделать реализацию, определяющую глубину сразу из нескольких источников
 * (например, левый и правый борт одновременно). Чтобы объекты более высокого уровня ничего
 * не знали о реально используемых источниках, используется этот идентификатор.
 * Строка должна содержать путь к БД, проект, галс и все внутренние параметры объекта.
 * Требуется, чтобы длина этой строки была постоянна (в пределах одного и того же) галса.
 * Поэтому значения внутренних параметров объекта нужно пропускать через некую хэш-функцию.
 *
 * Returns: указатель на строку с идентификатором.
 */
const gchar*
hyscan_nav_data_get_token (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), NULL);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_token != NULL)
    return (*iface->get_token) (navdata);

  return NULL;
}

/**
 * hyscan_nav_data_get_mod_count:
 * @ndata: указатель на интерфейс #HyScanNavData
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться на значение
 * номера изменения, важен только факт смены номера по сравнению с предыдущим запросом.
 * Изменениями считаются события в базе данных, связанные с этим каналом.
 *
 * Returns: значение счётчика изменений.
 */
guint32
hyscan_nav_data_get_mod_count (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), 0);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_mod_count != NULL)
    return (*iface->get_mod_count) (navdata);

  return 0;
}

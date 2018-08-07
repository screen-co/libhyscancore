/* hyscan-amplitude.c
 *
 * Copyright 2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

/**
 * SECTION: hyscan-amplitude
 * @Short_description: интерфейс доступа к амплитудным данным
 * @Title: HyScanAmplitude
 *
 * Интерфейс предназначен для универсального доступа к амплитудным данным
 * разных типов: боковой обзор, эхолот, профилограф и т.п.
 *
 * Функции #hyscan_amplitude_get_token, #hyscan_amplitude_get_position,
 * #hyscan_amplitude_get_info и #hyscan_amplitude_is_writable предназначены
 * для получения информации о канале данных и типе данных в нём.
 *
 * Функции #hyscan_amplitude_get_mod_count, #hyscan_amplitude_get_range и
 * #hyscan_amplitude_find_data предназначены для определения границ
 * записанных данных и их поиска по метке времени. Они аналогичны функциям
 * #hyscan_db_get_mod_count, #hyscan_db_channel_get_data_range и
 * #hyscan_db_channel_find_data интерфейса #HyScanDB.
 *
 * Для получения данных используются функции #hyscan_amplitude_get_size_time и
 * #hyscan_amplitude_get_amplitude.
 *
 * Классы, реализующие HyScanAmplitude, не поддерживают работу в многопоточном
 * режиме. Рекомендуется создавать свой экземпляр объекта обработки данных в
 * каждом потоке и использовать единый кэш данных.
 */

#include "hyscan-amplitude.h"

G_DEFINE_INTERFACE (HyScanAmplitude, hyscan_amplitude, G_TYPE_OBJECT)

static void
hyscan_amplitude_default_init (HyScanAmplitudeInterface *iface)
{
}

/**
 * hyscan_amplitude_get_token:
 * @amplitude: указатель на #HyScanAmplitude
 *
 * Функция возвращает строку с уникальным идентификатором. Этот идентификатор
 * может использоваться как основа для ключа кэширования в последующем.
 *
 * Returns: Уникальный идентификатор.
 */
const gchar *
hyscan_amplitude_get_token (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), NULL);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_token != NULL)
    return (* iface->get_token) (amplitude);

  return NULL;
}

/**
 * hyscan_amplitude_get_position:
 * @amplitude: указатель на #HyScanAmplitude
 *
 * Функция возвращает информацию о местоположении приёмной антенны
 * гидролокатора в виде значения структуры #HyScanAntennaPosition.
 *
 * Returns: Местоположение приёмной антенны.
 */
HyScanAntennaPosition
hyscan_amplitude_get_position (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeInterface *iface;
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), zero);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_position != NULL)
    return (* iface->get_position) (amplitude);

  return zero;
}

/**
 * hyscan_amplitude_get_info:
 * @amplitude: указатель на #HyScanAmplitude
 *
 * Функция возвращает параметры канала гидроакустических данных.
 *
 * Returns: Параметры канала гидроакустических данных.
 */
HyScanAcousticDataInfo
hyscan_amplitude_get_info (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeInterface *iface;
  HyScanAcousticDataInfo zero = {0};

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), zero);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_info != NULL)
    return (* iface->get_info) (amplitude);

  return zero;
}

/**
 * hyscan_amplitude_is_writable:
 * @amplitude: указатель на #HyScanAmplitude
 *
 * Функция определяет возможность изменения данных. Если функция вернёт
 * значение %TRUE, возможна ситуация когда могут появиться новые данные
 * или исчезнуть уже записанные.
 *
 * Returns: %TRUE если возможна запись данных, иначе %FALSE.
 */
gboolean
hyscan_amplitude_is_writable (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), FALSE);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->is_writable != NULL)
    return (* iface->is_writable) (amplitude);

  return FALSE;
}

/**
 * hyscan_amplitude_get_mod_count:
 * @amplitude: указатель на #HyScanAmplitude
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться
 * на значение номера изменения, важен только факт смены номера по сравнению с
 * предыдущим запросом.
 *
 * Returns: Номер изменения.
 */
guint32
hyscan_amplitude_get_mod_count (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), 0);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_mod_count != NULL)
    return (* iface->get_mod_count) (amplitude);

  return 0;
}

/**
 * hyscan_amplitude_get_range:
 * @amplitude: указатель на #HyScanAmplitude
 * @first_index: (nullable): начальный индекс данных
 * @last_index: (nullable): конечный индекс данных
 *
 * Функция возвращает диапазон значений индексов записанных данных. Данная
 * функция имеет логику работу аналогичную функции
 * #hyscan_db_channel_get_data_range.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_amplitude_get_range (HyScanAmplitude *amplitude,
                            guint32         *first_index,
                            guint32         *last_index)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), FALSE);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_range != NULL)
    return (* iface->get_range) (amplitude, first_index, last_index);

  return FALSE;
}

/**
 * hyscan_amplitude_find_data:
 * @amplitude: указатель на #HyScanAmplitude
 * @time: искомый момент времени
 * @lindex: (nullable): "левый" индекс данных
 * @rindex: (nullable): "правый" индекс данных
 * @ltime: (nullable): "левая" метка времени данных
 * @rtime: (nullable): "правая" метка времени данных
 *
 * Функция ищет индекс данных для указанного момента времени. Данная функция
 * имеет логику работу аналогичную функции #hyscan_db_channel_find_data.
 *
 * Returns: Статус поиска индекса данных.
 */
HyScanDBFindStatus
hyscan_amplitude_find_data (HyScanAmplitude *amplitude,
                            gint64           time,
                            guint32         *lindex,
                            guint32         *rindex,
                            gint64          *ltime,
                            gint64          *rtime)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), HYSCAN_DB_FIND_FAIL);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->find_data != NULL)
    return (* iface->find_data) (amplitude, time, lindex, rindex, ltime, rtime);

  return HYSCAN_DB_FIND_FAIL;
}

/**
 * hyscan_amplitude_get_size_time:
 * @amplitude: указатель на #HyScanAmplitude
 * @index: индекс считываемых данных
 * @n_points: (out) (nullable): число точек данных
 * @time: (out) (nullable): метка времени данных
 *
 * Функция возвращает число точек данных и метку времени для указанного
 * индекса данных.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_amplitude_get_size_time (HyScanAmplitude *amplitude,
                                guint32          index,
                                guint32         *n_points,
                                gint64          *time)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), FALSE);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_size_time != NULL)
    return (* iface->get_size_time) (amplitude, index, n_points, time);

  return FALSE;
}

/**
 * hyscan_amplitude_get_amplitude:
 * @amplitude: указатель на #HyScanAmplitude
 * @index: индекс считываемых данных
 * @n_points: (out): число точек данных
 * @time: (out) (nullable): метка времени данных
 * @noise: (out) (nullable): признак шумовых данных
 *
 * Функция возвращает массив значений амплитуды сигнала. Функция возвращает
 * указатель на внутренний буфер, данные в котором действительны до следующего
 * вызова этой функции. Пользователь не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Массив значений амплитуды сигнала или NULL.
 */
const gfloat *
hyscan_amplitude_get_amplitude (HyScanAmplitude *amplitude,
                                guint32          index,
                                guint32         *n_points,
                                gint64          *time,
                                gboolean        *noise)
{
  HyScanAmplitudeInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE (amplitude), NULL);

  iface = HYSCAN_AMPLITUDE_GET_IFACE (amplitude);
  if (iface->get_amplitude != NULL)
    return (* iface->get_amplitude) (amplitude, index, n_points, time, noise);

  return NULL;
}

/* hyscan-amplitude.h
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

#ifndef __HYSCAN_AMPLITUDE_H__
#define __HYSCAN_AMPLITUDE_H__

#include <hyscan-cache.h>
#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_AMPLITUDE            (hyscan_amplitude_get_type ())
#define HYSCAN_AMPLITUDE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_AMPLITUDE, HyScanAmplitude))
#define HYSCAN_IS_AMPLITUDE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_AMPLITUDE))
#define HYSCAN_AMPLITUDE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_AMPLITUDE, HyScanAmplitudeInterface))

typedef struct _HyScanAmplitude HyScanAmplitude;
typedef struct _HyScanAmplitudeInterface HyScanAmplitudeInterface;

/**
 * HyScanSonarInterface:
 * @g_iface: Базовый интерфейс.
 * @get_token: Функция возвращает уникальный идентификатор.
 * @get_position: Функция возвращает информацию о местоположении приёмной антенны.
 * @get_info: Функция возвращает параметры канала гидроакустических данных.
 * @is_writable: Функция определяет возможность изменения данных.
 * @get_mod_count: Функция возвращает номер изменения в данных.
 * @get_range: Функция возвращает диапазон значений индексов записанных данных.
 * @find_data: Функция ищет индекс данных для указанного момента времени.
 * @get_size_time: Функция возвращает число точек данных и метку времени.
 * @get_amplitude: Функция возвращает массив значений амплитуды сигнала.
 */
struct _HyScanAmplitudeInterface
{
  GTypeInterface         g_iface;

  const gchar *          (*get_token)                          (HyScanAmplitude       *amplitude);

  HyScanAntennaPosition  (*get_position)                       (HyScanAmplitude       *amplitude);

  HyScanAcousticDataInfo (*get_info)                           (HyScanAmplitude       *amplitude);

  gboolean               (*is_writable)                        (HyScanAmplitude       *amplitude);

  guint32                (*get_mod_count)                      (HyScanAmplitude       *amplitude);

  gboolean               (*get_range)                          (HyScanAmplitude       *amplitude,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

  HyScanDBFindStatus     (*find_data)                          (HyScanAmplitude       *amplitude,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

  gboolean               (*get_size_time)                      (HyScanAmplitude       *amplitude,
                                                                guint32                index,
                                                                guint32               *n_points,
                                                                gint64                *time);

  const gfloat *         (*get_amplitude)                      (HyScanAmplitude       *amplitude,
                                                                guint32                index,
                                                                guint32               *n_points,
                                                                gint64                *time,
                                                                gboolean              *noise);
};

HYSCAN_API
GType                  hyscan_amplitude_get_type               (void);

HYSCAN_API
const gchar *          hyscan_amplitude_get_token              (HyScanAmplitude       *amplitude);

HYSCAN_API
HyScanAntennaPosition  hyscan_amplitude_get_position           (HyScanAmplitude       *amplitude);

HYSCAN_API
HyScanAcousticDataInfo hyscan_amplitude_get_info               (HyScanAmplitude       *amplitude);

HYSCAN_API
gboolean               hyscan_amplitude_is_writable            (HyScanAmplitude       *amplitude);

HYSCAN_API
guint32                hyscan_amplitude_get_mod_count          (HyScanAmplitude       *amplitude);

HYSCAN_API
gboolean               hyscan_amplitude_get_range              (HyScanAmplitude       *amplitude,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

HYSCAN_API
HyScanDBFindStatus     hyscan_amplitude_find_data              (HyScanAmplitude       *amplitude,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

HYSCAN_API
gboolean               hyscan_amplitude_get_size_time          (HyScanAmplitude       *amplitude,
                                                                guint32                index,
                                                                guint32               *n_points,
                                                                gint64                *time);

HYSCAN_API
const gfloat *         hyscan_amplitude_get_amplitude          (HyScanAmplitude       *amplitude,
                                                                guint32                index,
                                                                guint32               *n_points,
                                                                gint64                *time,
                                                                gboolean              *noise);

G_END_DECLS

#endif /* __HYSCAN_AMPLITUDE_H__ */

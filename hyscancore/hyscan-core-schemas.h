/* hyscan-core-schemas.h
 *
 * Copyright 2016-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#ifndef __HYSCAN_CORE_SCHEMAS_H__
#define __HYSCAN_CORE_SCHEMAS_H__

#define PROJECT_INFO_SCHEMA_ID                 1495752243900067309
#define PROJECT_INFO_SCHEMA_VERSION            20180100

#define TRACK_INFO_SCHEMA_ID                   3829672927190415735
#define TRACK_INFO_SCHEMA_VERSION              2020424

#define TRACK_SCHEMA_ID                        1715033709558529337
#define TRACK_SCHEMA_VERSION                   20200300

#define LOG_SCHEMA_ID                          3957463010395734756
#define LOG_SCHEMA_VERSION                     20190100

#define SENSOR_CHANNEL_SCHEMA_ID               5474665919775311270
#define SENSOR_CHANNEL_SCHEMA_VERSION          20190100

#define ACOUSTIC_CHANNEL_SCHEMA_ID             3533456721320349085
#define ACOUSTIC_CHANNEL_SCHEMA_VERSION        20200200

#define SIGNAL_CHANNEL_SCHEMA_ID               4522835908161425227
#define SIGNAL_CHANNEL_SCHEMA_VERSION          20190100

#define TVG_CHANNEL_SCHEMA_ID                  8911020404930317035
#define TVG_CHANNEL_SCHEMA_VERSION             20190100

#define WATERFALL_MARK_SCHEMA_ID               1315931457526726065
#define WATERFALL_MARK_SCHEMA_VERSION          20190200

#define GEO_MARK_SCHEMA_ID                     4566818919687782529
#define GEO_MARK_SCHEMA_VERSION                20190100

#define PLANNER_ORIGIN_SCHEMA_ID               3906613780672057459
#define PLANNER_ORIGIN_SCHEMA_VERSION          20190100

#define PLANNER_ZONE_SCHEMA_ID                 2298441576805697181
#define PLANNER_ZONE_SCHEMA_VERSION            20190100

#define PLANNER_TRACK_SCHEMA_ID                1788376350812305657
#define PLANNER_TRACK_SCHEMA_VERSION           20200300

#define LABEL_SCHEMA_ID                        5468681196977785233
#define LABEL_SCHEMA_VERSION                   20200113

#define PROJECT_INFO_SCHEMA                    "project-info"
#define TRACK_INFO_SCHEMA                      "track-info"

#define TRACK_SCHEMA                           "track"
#define LOG_CHANNEL_SCHEMA                     "log"
#define SENSOR_CHANNEL_SCHEMA                  "sensor"
#define ACOUSTIC_CHANNEL_SCHEMA                "acoustic"
#define SIGNAL_CHANNEL_SCHEMA                  "signal"
#define TVG_CHANNEL_SCHEMA                     "tvg"

#define WATERFALL_MARK_SCHEMA                  "waterfall-mark"
#define GEO_MARK_SCHEMA                        "geo-mark"

#define PROJECT_INFO_GROUP                     "info"
#define PROJECT_INFO_OBJECT                    "project"

#define PLANNER_ORIGIN_SCHEMA                  "planner-origin"
#define PLANNER_ZONE_SCHEMA                    "planner-zone"
#define PLANNER_TRACK_SCHEMA                   "planner-track"
#define PLANNER_OBJECT                         "planner"

#define LABEL_SCHEMA                           "label"

#endif /* __HYSCAN_CORE_SCHEMAS_H__ */

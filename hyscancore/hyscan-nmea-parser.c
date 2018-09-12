/* hyscan-nmea-parser.c
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
 * SECTION: hyscan-nmea-parser
 * @Short_description: класс парсинга NMEA-строк
 * @Title: HyScanNMEAParser
 *
 * Класс HyScanNMEAParser реализует интерфейс #HyScanNavData.
 * Поскольку интерфейс HyScanNavData возвращает только одно значение,
 * нельзя вернуть значение+полушарие или день/минуту/секунду.
 * Поэтому все значения приводятся к одному числу.
 * Время в unix-time, западное и южное полушария - со знаком минус.
 */

#include "hyscan-nmea-parser.h"
#include <hyscan-nmea-data.h>
#include <string.h>
#include <math.h>

struct HyScanFieldFunc
{
  gint       RMC_field_n;
  gint       GGA_field_n;
  gint       DPT_field_n;
  gboolean (*func) (const gchar *sentence,
                    gdouble     *value);
};


enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_SOURCE_CHANNEL,
  PROP_SOURCE_TYPE,
  PROP_FIELD_TYPE
};

struct _HyScanNMEAParserPrivate
{
  /* Переменные, передающиеся на этапе создания объекта. */
  HyScanDB              *db;            /* База данных. */
  gchar                 *project;       /* Имя проекта. */
  gchar                 *track;         /* Имя галса. */
  HyScanSourceType       source_type;   /* Тип данных. */
  guint                  channel_n;      /* Номер канала для используемого типа данных. */
  HyScanNMEAField        field_type;    /* Тип анализируемых данных. */

  /* Переменные, определяющиеся на этапе конструирования. */
  HyScanNMEAData        *dc;            /* Канал данных. */
  gchar                 *token;         /* Токен. */

  HyScanAntennaPosition  position;      /* Местоположение приемника. */

  /* Параметры лексического анализа. */
  gint                   field_n;
  gboolean             (*parse_func) (const gchar *sentence,
                                      gdouble     *value);
};

static void     hyscan_nmea_parser_interface_init       (HyScanNavDataInterface  *iface);
static void     hyscan_nmea_parser_set_property         (GObject                 *object,
                                                        guint                     prop_id,
                                                        const GValue             *value,
                                                        GParamSpec               *pspec);
static void     hyscan_nmea_parser_object_constructed   (GObject                 *object);
static void     hyscan_nmea_parser_object_finalize      (GObject                 *object);
static gboolean hyscan_nmea_parser_setup                (HyScanNMEAParserPrivate *priv);
static const gchar * hyscan_nmea_parser_shift           (const gchar             *sentence,
                                                         guint                    field);
static gboolean hyscan_nmea_parser_parse_value          (const gchar             *sentence,
                                                         gdouble                 *val);
static gboolean hyscan_nmea_parser_parse_time           (const gchar             *sentence,
                                                         gdouble                 *val);
static gboolean hyscan_nmea_parser_parse_date           (const gchar             *sentence,
                                                         gdouble                 *val);
static gboolean hyscan_nmea_parser_parse_latlon         (const gchar             *sentence,
                                                         gdouble                 *val);
static gboolean hyscan_nmea_parser_parse_meters         (const gchar             *sentence,
                                                         gdouble                 *val);

G_DEFINE_TYPE_WITH_CODE (HyScanNMEAParser, hyscan_nmea_parser, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNMEAParser)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_NAV_DATA, hyscan_nmea_parser_interface_init));

static void
hyscan_nmea_parser_class_init (HyScanNMEAParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nmea_parser_set_property;
  object_class->constructed = hyscan_nmea_parser_object_constructed;
  object_class->finalize = hyscan_nmea_parser_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
      g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "project name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track", "TrackName", "track name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_CHANNEL,
      g_param_spec_int ("source-channel", "SourceChannel", "Source channel", 1, 5, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_TYPE,
      g_param_spec_int ("source-type", "SourceType", "Source type", 0, G_MAXINT, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_FIELD_TYPE,
      g_param_spec_int ("field-type", "FieldType", "Data to obtain", -1, G_MAXINT, -1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_nmea_parser_init (HyScanNMEAParser *parser)
{
  parser->priv = hyscan_nmea_parser_get_instance_private (parser);
}

static void
hyscan_nmea_parser_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (object);
  HyScanNMEAParserPrivate *priv = parser->priv;

  if (prop_id == PROP_DB)
    priv->db = g_value_dup_object (value);
  else if (prop_id == PROP_PROJECT)
    priv->project = g_value_dup_string (value);
  else if (prop_id == PROP_TRACK)
    priv->track = g_value_dup_string (value);
  else if (prop_id == PROP_SOURCE_CHANNEL)
    priv->channel_n = g_value_get_int (value);
  else if (prop_id == PROP_SOURCE_TYPE)
    priv->source_type = g_value_get_int (value);
  else if (prop_id == PROP_FIELD_TYPE)
    priv->field_type = g_value_get_int (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (parser, prop_id, pspec);
}

static void
hyscan_nmea_parser_object_constructed (GObject *object)
{
  gchar *db_uri;
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (object);
  HyScanNMEAParserPrivate *priv = parser->priv;

  /* Открываем КД. */
  priv->dc = hyscan_nmea_data_new (priv->db, priv->project, priv->track,
                                   priv->source_type, priv->channel_n);

  if (priv->dc == NULL)
    return;

  /* Генерируем токен. */
  db_uri = hyscan_db_get_uri (priv->db);
  priv->token = g_strdup_printf ("nmea_parser.%s.%s.%s.%i.%i",
                                 db_uri, priv->project, priv->track,
                                 priv->source_type,
                                 priv->channel_n);

  if (!hyscan_nmea_parser_setup (priv))
    g_clear_object (&priv->dc);

  g_free (db_uri);
}

static void
hyscan_nmea_parser_object_finalize (GObject *object)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (object);
  HyScanNMEAParserPrivate *priv = parser->priv;

  g_free (priv->project);
  g_free (priv->track);

  g_clear_object (&priv->dc);

  g_clear_object (&priv->db);
  g_free (priv->token);

  G_OBJECT_CLASS (hyscan_nmea_parser_parent_class)->finalize (object);
}

static gboolean
hyscan_nmea_parser_setup (HyScanNMEAParserPrivate *priv)
{
  struct HyScanFieldFunc fields[] =
  {
    /* .RMC_field_n, .GGA_field_n, .DPT_field_n, .func */
    { 1,   1,  -1, hyscan_nmea_parser_parse_time},   /* HYSCAN_NMEA_FIELD_TIME      */
    { 3,   2,  -1, hyscan_nmea_parser_parse_latlon}, /* HYSCAN_NMEA_FIELD_LAT       */
    { 5,   4,  -1, hyscan_nmea_parser_parse_latlon}, /* HYSCAN_NMEA_FIELD_LON       */
    { 7,  -1,  -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_SPEED     */
    { 8,  -1,  -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_TRACK     */
    { 9,  -1,  -1, hyscan_nmea_parser_parse_date},   /* HYSCAN_NMEA_FIELD_DATE      */
    { 10, -1,  -1, hyscan_nmea_parser_parse_meters}, /* HYSCAN_NMEA_FIELD_MAG_VAR   */
    {-1,   6,  -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_FIX_QUAL  */
    {-1,   7,  -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_N_SATS    */
    {-1,   8,  -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_HDOP      */
    {-1,   9,  -1, hyscan_nmea_parser_parse_meters}, /* HYSCAN_NMEA_FIELD_ALTITUDE  */
    {-1,   11, -1, hyscan_nmea_parser_parse_meters}, /* HYSCAN_NMEA_FIELD_HOG       */
    {-1,  -1,   1, hyscan_nmea_parser_parse_value}   /* HYSCAN_NMEA_FIELD_DEPTH     */
  };

  priv->parse_func = fields[priv->field_type].func;

  if (priv->source_type == HYSCAN_SOURCE_NMEA_RMC)
    priv->field_n = fields[priv->field_type].RMC_field_n;
  if (priv->source_type == HYSCAN_SOURCE_NMEA_GGA)
    priv->field_n = fields[priv->field_type].GGA_field_n;
  if (priv->source_type == HYSCAN_SOURCE_NMEA_DPT)
    priv->field_n = fields[priv->field_type].DPT_field_n;

  if (priv->field_n == -1)
    return FALSE;

  return TRUE;
}

static const gchar *
hyscan_nmea_parser_shift (const gchar *sentence,
                          guint        field)
{
  for (; sentence != NULL && field > 0; --field)
    sentence = strchr (sentence, ',') + 1;

  return sentence;
}

/* Функция парсит. */
static gboolean
hyscan_nmea_parser_parse_value (const gchar *sentence,
                                gdouble     *_val)
{
  gchar *end;
  gdouble val;

  val = g_ascii_strtod (sentence, &end);

  if (val == 0 && end == sentence)
    return FALSE;

  *_val = val;
  return TRUE;
}

/* Функция парсит. */
static gboolean
hyscan_nmea_parser_parse_date (const gchar *sentence,
                               gdouble     *_val)
{
  GDateTime *dt;

  gint year, month, day;
  gdouble val;

  /* Если строка нулевая или пустая. */
  if (sentence == NULL || sentence[0] == ',')
    return FALSE;

  day =   sentence[0] * 10 + sentence[1];
  month = sentence[2] * 10 + sentence[3];
  year =  sentence[4] * 10 + sentence[5];

  dt = g_date_time_new_utc (year, month, day, 0, 0, 0);
  val = g_date_time_to_unix (dt);
  g_date_time_unref (dt);

  *_val = val;
  return TRUE;
}

/* Функция парсит. */
static gboolean
hyscan_nmea_parser_parse_time (const gchar *sentence,
                               gdouble     *_val)
{
  GDateTime *dt;

  gint hour, min;
  gdouble sec, val;

  /* Если строка нулевая или пустая. */
  if (sentence == NULL || sentence[0] == ',')
    return FALSE;

  hour = sentence[0] * 10 + sentence[1];
  min =  sentence[2] * 10 + sentence[3];
  sec =  sentence[4] * 10 + sentence[5];

  dt = g_date_time_new_utc (0, 0, 0, hour, min, sec);
  val = g_date_time_to_unix (dt);
  g_date_time_unref (dt);

  *_val = val;
  return TRUE;
}

/* Функция парсит. */
static gboolean
hyscan_nmea_parser_parse_latlon (const gchar *sentence,
                                 gdouble     *_val)
{
  gchar *end, side;
  gdouble val, deg, min;

  val = g_ascii_strtod (sentence, &end);

  if (val == 0 && end == sentence)
    return FALSE;

  /* Переводим в градусы десятичные. */
                             /* Пусть на входе 5530.671 */
  //val /= 100.0;            /* 55.30671 */
  deg = floor (val / 100.0); /* 55 */
  min = val - deg * 100;     /* 30.671 */
  min /= 60.0;               /* 30.671 -> 0.5111 */
  val = deg + min;           /* 55.511 */

  side = *(end + 1);

  /* Южное и западное полушарие со знаком минус. */
  if (side == 'S' || side == 'W')
    val *= -1;

  *_val = val;
  return TRUE;
}

/* Функция парсит. */
static gboolean
hyscan_nmea_parser_parse_meters (const gchar *sentence,
                                 gdouble     *_val)
{
  gchar *end;
  gdouble val;

  val = g_ascii_strtod (sentence, &end);

  if (val == 0 && end == sentence)
    return FALSE;

  /* Футы переводим в метры. */
  if (*(end+1) == 'f')
    val *= 0.3048;

  *_val = val;
  return TRUE;
}

/* Функция установки кэша. */
static void
hyscan_nmea_parser_set_cache (HyScanNavData *navdata,
                              HyScanCache   *cache)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  hyscan_nmea_data_set_cache (parser->priv->dc, cache);
}

/* Функция получения глубины. */
static gboolean
hyscan_nmea_parser_get (HyScanNavData *navdata,
                        guint32        index,
                        gint64        *time,
                        gdouble       *value)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  HyScanNMEAParserPrivate *priv = parser->priv;

  const gchar *nmea_sentence;
  gdouble nmea_value;
  gint64 nmea_time;

  /* Забираем строку из БД (или кэша, как повезет) и парсим её. */
  nmea_sentence = hyscan_nmea_data_get_sentence (priv->dc, index, &nmea_time);

  nmea_sentence = hyscan_nmea_parser_shift (nmea_sentence, priv->field_n);
  if (!(priv->parse_func) (nmea_sentence, &nmea_value))
    return FALSE;

  if (time != NULL)
    *time = nmea_time;
  if (value != NULL)
    *value = nmea_value;

  return TRUE;
}

/* Функция поиска данных. */
static HyScanDBFindStatus
hyscan_nmea_parser_find_data (HyScanNavData *navdata,
                              gint64         time,
                              guint32       *lindex,
                              guint32       *rindex,
                              gint64        *ltime,
                              gint64        *rtime)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return hyscan_nmea_data_find_data (parser->priv->dc, time, lindex, rindex, ltime, rtime);
}

/* Функция определения диапазона. */
static gboolean
hyscan_nmea_parser_get_range (HyScanNavData *navdata,
                              guint32       *first,
                              guint32       *last)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return hyscan_nmea_data_get_range (parser->priv->dc, first, last);
}

/* Функция получения местоположения антенны. */
static HyScanAntennaPosition
hyscan_nmea_parser_get_position (HyScanNavData *navdata)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return hyscan_nmea_data_get_position (parser->priv->dc);
}

/* Функция определяет, возможна ли дозапись в канал данных. */
static gboolean
hyscan_nmea_parser_is_writable (HyScanNavData *navdata)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return hyscan_nmea_data_is_writable (parser->priv->dc);
}

/* Функция возвращает токен объекта. */
static const gchar*
hyscan_nmea_parser_get_token (HyScanNavData *navdata)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return parser->priv->token;
}

/* Функция возвращает значение счётчика изменений. */
static guint32
hyscan_nmea_parser_get_mod_count (HyScanNavData *navdata)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return hyscan_nmea_data_get_mod_count (parser->priv->dc);
}

/**
 * hyscan_nmea_parser_new:
 * @db: указатель на интерфейс HyScanDB
 * @project: название проекта
 * @track: название галса
 * @source_channel: номер канала
 *
 * Функция создает новый объект обработки строк NMEA.
 *
 * Returns: Указатель на объект #HyScanNMEAParser или %NULL
 *
 */
HyScanNMEAParser*
hyscan_nmea_parser_new (HyScanDB        *db,
                        const gchar     *project,
                        const gchar     *track,
                        HyScanSourceType source_type,
                        guint            source_channel,
                        guint            field_type)
{
  HyScanNMEAParser *parser;

  parser = g_object_new (HYSCAN_TYPE_NMEA_PARSER,
                         "db", db,
                         "project", project,
                         "track", track,
                         "source-channel", source_channel,
                         "source-type", source_type,
                         "field-type", field_type,
                         NULL);

  if (parser->priv->dc == NULL)
    g_clear_object (&parser);

  return parser;
}

static void
hyscan_nmea_parser_interface_init (HyScanNavDataInterface *iface)
{
  iface->set_cache     = hyscan_nmea_parser_set_cache;
  iface->get           = hyscan_nmea_parser_get;
  iface->find_data     = hyscan_nmea_parser_find_data;
  iface->get_range     = hyscan_nmea_parser_get_range;
  iface->get_position  = hyscan_nmea_parser_get_position;
  iface->get_token     = hyscan_nmea_parser_get_token;
  iface->is_writable   = hyscan_nmea_parser_is_writable;
  iface->get_mod_count = hyscan_nmea_parser_get_mod_count;
}
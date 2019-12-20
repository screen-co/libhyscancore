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
#include "hyscan-track-data.h"
#include <hyscan-nmea-data.h>
#include <string.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_CACHE,
  PROP_DB,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_SOURCE_CHANNEL,
  PROP_DATA_TYPE,
  PROP_FIELD_TYPE
};

struct _HyScanNMEAParserPrivate
{
  /* Переменные, передающиеся на этапе создания объекта. */
  HyScanDB              *db;            /* База данных. */
  HyScanCache           *cache;         /* Кэш. */
  gchar                 *project;       /* Имя проекта. */
  gchar                 *track;         /* Имя галса. */
  HyScanNmeaDataType     data_type;     /* Тип данных. */

  guint                  channel_n;     /* Номер канала для используемого типа данных. */
  HyScanNMEAField        field_type;    /* Тип анализируемых данных. */

  /* Переменные, определяющиеся на этапе конструирования. */
  HyScanNMEAData        *dc;            /* Канал данных. */
  gchar                 *token;         /* Токен. */

  HyScanAntennaOffset    offset;        /* Смещение антенны приемника. */

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
static gboolean hyscan_nmea_parser_parse_helper         (HyScanNMEAParserPrivate *priv,
                                                         const gchar             *sentence,
                                                         gdouble                 *value);

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

  g_object_class_install_property (object_class, PROP_CACHE,
      g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "project name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track", "TrackName", "track name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_CHANNEL,
      g_param_spec_uint ("source-channel", "SourceChannel", "Source channel", 1, G_MAXUINT, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_DATA_TYPE,
      g_param_spec_int ("data-type", "DataType", "Desired string type", HYSCAN_NMEA_DATA_INVALID, G_MAXINT, 0,
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
  else if (prop_id == PROP_CACHE)
    priv->cache = g_value_dup_object (value);
  else if (prop_id == PROP_PROJECT)
    priv->project = g_value_dup_string (value);
  else if (prop_id == PROP_TRACK)
    priv->track = g_value_dup_string (value);
  else if (prop_id == PROP_SOURCE_CHANNEL)
    priv->channel_n = g_value_get_uint (value);
  else if (prop_id == PROP_DATA_TYPE)
    priv->data_type = g_value_get_int (value);
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

  /* Настраиваем функции парсинга. */
  if (!hyscan_nmea_parser_setup (priv))
    return;

  /* Если не заданы бд, проект и галс, выходим, это пустой парсер. */
  if (priv->db == NULL && priv->project == NULL && priv->track == NULL)
    return;

  priv->dc = hyscan_nmea_data_new (priv->db, priv->cache,
                                   priv->project, priv->track,
                                   priv->channel_n);

  if (priv->dc == NULL)
    return;

  /* Генерируем токен. */
  db_uri = hyscan_db_get_uri (priv->db);
  priv->token = g_strdup_printf ("nmea_parser.%s.%s.%s.%i.%i",
                                 db_uri, priv->project, priv->track,
                                 priv->data_type,
                                 priv->channel_n);

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

  g_clear_object (&priv->cache);
  g_clear_object (&priv->db);
  g_free (priv->token);

  G_OBJECT_CLASS (hyscan_nmea_parser_parent_class)->finalize (object);
}

static gboolean
hyscan_nmea_parser_setup (HyScanNMEAParserPrivate *priv)
{
  struct HyScanFieldFunc
  {
    gint       RMC;
    gint       GGA;
    gint       DPT;
    gint       HDT;
    gint       HPR;
    gboolean (*func) (const gchar *sentence,
                      gdouble     *value);
  } fields[] =
  {
    /* .RMC, .GGA, .DPT, .HDT, .HPR, .func */
    { 1,   1,  -1, -1, -1, hyscan_nmea_parser_parse_time},   /* HYSCAN_NMEA_FIELD_TIME      */
    { 3,   2,  -1, -1, -1, hyscan_nmea_parser_parse_latlon}, /* HYSCAN_NMEA_FIELD_LAT       */
    { 5,   4,  -1, -1, -1, hyscan_nmea_parser_parse_latlon}, /* HYSCAN_NMEA_FIELD_LON       */
    { 7,  -1,  -1, -1, -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_SPEED     */
    { 8,  -1,  -1, -1, -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_TRACK     */
    {-1,  -1,  -1,  1,  1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_HEADING   */
    { 9,  -1,  -1, -1, -1, hyscan_nmea_parser_parse_date},   /* HYSCAN_NMEA_FIELD_DATE      */
    { 10, -1,  -1, -1, -1, hyscan_nmea_parser_parse_meters}, /* HYSCAN_NMEA_FIELD_MAG_VAR   */
    {-1,   6,  -1, -1, -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_FIX_QUAL  */
    {-1,   7,  -1, -1, -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_N_SATS    */
    {-1,   8,  -1, -1, -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_HDOP      */
    {-1,   9,  -1, -1, -1, hyscan_nmea_parser_parse_meters}, /* HYSCAN_NMEA_FIELD_ALTITUDE  */
    {-1,   11, -1, -1, -1, hyscan_nmea_parser_parse_meters}, /* HYSCAN_NMEA_FIELD_HOG       */
    {-1,  -1,   1, -1, -1, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_DEPTH     */
    {-1,  -1,  -1, -1,  2, hyscan_nmea_parser_parse_value},  /* HYSCAN_NMEA_FIELD_PITCH     */
    {-1,  -1,  -1, -1,  3, hyscan_nmea_parser_parse_value}   /* HYSCAN_NMEA_FIELD_ROLL      */
  };

  priv->parse_func = fields[priv->field_type].func;

  if (priv->data_type == HYSCAN_NMEA_DATA_RMC)
    priv->field_n = fields[priv->field_type].RMC;
  else if (priv->data_type == HYSCAN_NMEA_DATA_GGA)
    priv->field_n = fields[priv->field_type].GGA;
  else if (priv->data_type == HYSCAN_NMEA_DATA_DPT)
    priv->field_n = fields[priv->field_type].DPT;
  else if (priv->data_type == HYSCAN_NMEA_DATA_HDT)
    priv->field_n = fields[priv->field_type].HDT;
  else if (priv->data_type == HYSCAN_NMEA_DATA_HYHPR)
    priv->field_n = fields[priv->field_type].HPR;

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

/* Функция парсит просто вещественное значение. */
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

/* Функция парсит дату. */
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

  day =   (sentence[0]-'0') * 10 + (sentence[1]-'0');
  month = (sentence[2]-'0') * 10 + (sentence[3]-'0');
  year =  (sentence[4]-'0') * 10 + (sentence[5]-'0');

  dt = g_date_time_new_utc (2000 + year, month, day, 0, 0, 0);
  val = g_date_time_to_unix (dt);
  g_date_time_unref (dt);

  *_val = val;
  return TRUE;
}

/* Функция парсит время. */
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

  hour = (sentence[0]-'0') * 10 + (sentence[1]-'0');
  min =  (sentence[2]-'0') * 10 + (sentence[3]-'0');
  sec = g_ascii_strtod (sentence + 4, NULL);

  dt = g_date_time_new_utc (1970, 1, 1, hour, min, sec);
  if (dt == NULL)
    return FALSE;
  val = g_date_time_to_unix (dt);
  val += g_date_time_get_microsecond (dt) / 1e6;
  g_date_time_unref (dt);

  *_val = val;
  return TRUE;
}

/* Функция парсит координаты. */
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
  deg = floor (val / 100.0); /* 55.30671, потом 55 */
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

/* Функция парсит метры. */
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

/* Обертка над парсером. */
static gboolean
hyscan_nmea_parser_parse_helper (HyScanNMEAParserPrivate *priv,
                                 const gchar             *sentence,
                                 gdouble                 *value)
{
  const gchar *signature;
  gssize go_back = 0;

  /* Если строки нет, просто выходим. */
  if (sentence == NULL)
    return FALSE;

  switch (priv->data_type)
    {
    case HYSCAN_NMEA_DATA_RMC:
      signature = "RMC";
      go_back = 3;
      break;
    case HYSCAN_NMEA_DATA_GGA:
      signature = "GGA";
      go_back = 3;
      break;
    case HYSCAN_NMEA_DATA_DPT:
      signature = "DPT";
      go_back = 3;
      break;
    case HYSCAN_NMEA_DATA_HDT:
      signature = "HDT";
      go_back = 3;
      break;
    case HYSCAN_NMEA_DATA_HYHPR:
      signature = "HYHPR";
      go_back = 1;
      break;
    default:
      signature = "$";
      break;
    }

  sentence = g_strstr_len (sentence, -1, signature);
  if (sentence == NULL)
      return FALSE;

  sentence -= go_back;

  /* Ищем нужное поле. Если вернулся NULL, выходим. */
  sentence = hyscan_nmea_parser_shift (sentence, priv->field_n);
  if (sentence == NULL)
    return FALSE;

  /* Теперь парсим строчку и возвращаем результат. */
  return (priv->parse_func)(sentence, value);
}

/* Функция получения глубины. */
static gboolean
hyscan_nmea_parser_get (HyScanNavData     *navdata,
                        HyScanCancellable *cancellable,
                        guint32            index,
                        gint64            *time,
                        gdouble           *value)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  HyScanNMEAParserPrivate *priv = parser->priv;

  const gchar *record;
  gdouble nmea_value;
  gint64 nmea_time;

  /* Забираем строку из БД (или кэша, как повезет) и парсим её. */
  record = hyscan_nmea_data_get (priv->dc, index, &nmea_time);

  if (!hyscan_nmea_parser_parse_helper (priv, record, &nmea_value))
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

/* Функция получения смещения антенны. */
static HyScanAntennaOffset
hyscan_nmea_parser_get_offset (HyScanNavData *navdata)
{
  HyScanNMEAParser *parser = HYSCAN_NMEA_PARSER (navdata);
  return hyscan_nmea_data_get_offset (parser->priv->dc);
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
 * @cache: указатель на интерфейс HyScanCache
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
                        HyScanCache     *cache,
                        const gchar     *project,
                        const gchar     *track,
                        guint            source_channel,
                        HyScanNmeaDataType data_type,
                        guint            field_type)
{
  HyScanNMEAParser *parser;

#ifdef HYSCAN_GGA_HACK
  if (field_type == HYSCAN_NMEA_FIELD_TRACK)
    {
      HyScanNMEAParser *lat_parser, *lon_parser;
      HyScanNavData *track_data;

      lat_parser = hyscan_nmea_parser_new (db, cache, project, track, source_channel, HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_LAT);
      lon_parser = hyscan_nmea_parser_new (db, cache, project, track, source_channel, HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_LON);
      track_data = hyscan_track_data_new (HYSCAN_NAV_DATA (lat_parser), HYSCAN_NAV_DATA (lon_parser));

      g_object_unref (lat_parser);
      g_object_unref (lon_parser);

      return (HyScanNMEAParser *) track_data;
    }
    if (data_type == HYSCAN_NMEA_DATA_RMC)
      data_type = HYSCAN_NMEA_DATA_GGA;
#endif

  parser = g_object_new (HYSCAN_TYPE_NMEA_PARSER,
                         "db", db,
                         "cache", cache,
                         "project", project,
                         "track", track,
                         "source-channel", source_channel,
                         "data-type", data_type,
                         "field-type", field_type,
                         NULL);

  if (parser->priv->dc == NULL)
    g_clear_object (&parser);

  return parser;
}

HyScanNMEAParser*
hyscan_nmea_parser_new_empty (HyScanNmeaDataType data_type,
                              guint              field_type)
{
  return g_object_new (HYSCAN_TYPE_NMEA_PARSER,
                       "data-type", data_type,
                       "field-type", field_type,
                       NULL);
}

gboolean
hyscan_nmea_parser_parse_string (HyScanNMEAParser *parser,
                                 const gchar      *string,
                                 gdouble          *value)
{
  gdouble nmea_value;
  HyScanNMEAParserPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_NMEA_PARSER (parser), FALSE);
  priv = parser->priv;

  if (HYSCAN_NMEA_DATA_INVALID == hyscan_nmea_data_check_sentence (string))
    {
      g_message ("Broken string <%s>", string);
      return FALSE;
    }

  if (!hyscan_nmea_parser_parse_helper (priv, string, &nmea_value))
    return FALSE;

  if (value != NULL)
    *value = nmea_value;

  return TRUE;
}

static void
hyscan_nmea_parser_interface_init (HyScanNavDataInterface *iface)
{
  iface->get           = hyscan_nmea_parser_get;
  iface->find_data     = hyscan_nmea_parser_find_data;
  iface->get_range     = hyscan_nmea_parser_get_range;
  iface->get_offset    = hyscan_nmea_parser_get_offset;
  iface->get_token     = hyscan_nmea_parser_get_token;
  iface->is_writable   = hyscan_nmea_parser_is_writable;
  iface->get_mod_count = hyscan_nmea_parser_get_mod_count;
}

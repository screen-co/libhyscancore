/*
 * \file hyscan-tile-color.c
 *
 * \brief Исходный файл класса раскрашивания тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * Цветовые схемы хранятся в структурах HyScanTileColorInfo.
 * Сама цветовая схема хранится в GArray по той причине, что
 * класс допускает множественный доступ. И пока один поток разукрашивает
 * тайл одним цветом, другой поток может поменять цветовую схему.
 *
 */

#include "hyscan-tile-color.h"
#include <math.h>
#include <string.h>
#include <zlib.h>

/* Макрос для поиска элементов в одномерном массиве. */
#define HYSCAN_COLOR_AT(i, j) *((guint32*)(data + (i) * (stride) + (j) * sizeof (guint32)))

#define TILE_COLOR_MAGIC 0x1983390d

enum
{
  PROP_0,
  PROP_CACHE
};

typedef struct
{
  guint32    magic;
  guint32    size;
  HyScanTile tile;
} HyScanTileColorCache;

typedef struct
{
  gfloat       black;          /* Черная точка. */
  gfloat       gamma;          /* Белая точка. */
  gfloat       white;          /* Гамма. */

  GArray      *colormap;       /* Цветовая схема. */
  guint32      background;     /* Фон. */

  gchar       *mnemonic;       /* Строка, содержащая параметры раскрашивания. */
} HyScanTileColorInfo;

struct _HyScanTileColorPrivate
{
  GMutex        setup_lock;
  HyScanCache  *cache;          /* Кэш. */
  gchar        *path;           /* Префикс системы кэширования (составленный из БД/проекта/галса). */

  GHashTable   *colorinfos;
  GMutex        lock;

  HyScanBuffer *write1;
  HyScanBuffer *write2;
  HyScanBuffer *read1;
  HyScanBuffer *read2;
};

static void     hyscan_tile_color_set_property                 (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);

static void     hyscan_tile_color_object_constructed           (GObject                *object);
static void     hyscan_tile_color_object_finalize              (GObject                *object);

static HyScanTileColorInfo *hyscan_tile_color_info_new         (void);
static HyScanTileColorInfo *hyscan_tile_color_info_lookup      (HyScanTileColorPrivate *priv,
                                                                HyScanSourceType        source);
static void     hyscan_tile_color_destroy_info                 (gpointer                data);

static gboolean hyscan_tile_color_set_levels_internal          (HyScanTileColor        *color,
                                                                HyScanSourceType        source,
                                                                gdouble                 black,
                                                                gdouble                 gamma,
                                                                gdouble                 white);
static gboolean hyscan_tile_color_set_colormap_internal        (HyScanTileColor        *color,
                                                                HyScanSourceType        source,
                                                                guint32                *colormap,
                                                                guint                   length,
                                                                guint32                 background);
static gchar   *hyscan_tile_color_cache_key                    (HyScanTile             *tile,
                                                                const gchar            *mnemonic,
                                                                const gchar            *path);
static void     hyscan_tile_color_update_mnemonic              (HyScanTileColorInfo    *cmap);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTileColor, hyscan_tile_color, G_TYPE_OBJECT);

static void
hyscan_tile_color_class_init (HyScanTileColorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_tile_color_set_property;
  object_class->constructed = hyscan_tile_color_object_constructed;
  object_class->finalize = hyscan_tile_color_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_tile_color_init (HyScanTileColor *color)
{
  color->priv = hyscan_tile_color_get_instance_private (color);
}

static void
hyscan_tile_color_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanTileColor *color = HYSCAN_TILE_COLOR (object);
  HyScanTileColorPrivate *priv = color->priv;

  if (prop_id == PROP_CACHE)
    priv->cache = g_value_dup_object (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_tile_color_object_constructed (GObject *object)
{
  HyScanTileColor *color = HYSCAN_TILE_COLOR (object);
  HyScanTileColorPrivate *priv = color->priv;
  HyScanTileColorInfo *cmap;

  g_mutex_init (&priv->lock);
  g_mutex_init (&priv->setup_lock);
  priv->colorinfos = g_hash_table_new_full (NULL, NULL, NULL, hyscan_tile_color_destroy_info);

  /* Создаем цветовую схему по умолчанию. */
  cmap = hyscan_tile_color_info_new ();
  g_hash_table_insert (priv->colorinfos, GINT_TO_POINTER (HYSCAN_SOURCE_INVALID), cmap);

  priv->write1 = hyscan_buffer_new ();
  priv->write2 = hyscan_buffer_new ();
  priv->read1 = hyscan_buffer_new ();
  priv->read2 = hyscan_buffer_new ();
}

static void
hyscan_tile_color_object_finalize (GObject *object)
{
  HyScanTileColor *color = HYSCAN_TILE_COLOR (object);
  HyScanTileColorPrivate *priv = color->priv;

  g_clear_object (&priv->cache);

  g_hash_table_unref (priv->colorinfos);
  g_mutex_clear (&priv->lock);
  g_mutex_clear (&priv->setup_lock);

  g_free (priv->path);

  g_clear_object (&priv->write1);
  g_clear_object (&priv->write2);
  g_clear_object (&priv->read1);
  g_clear_object (&priv->read2);

  G_OBJECT_CLASS (hyscan_tile_color_parent_class)->finalize (object);
}

/* Функция создает структуру с параметрами разукрашивания. */
static HyScanTileColorInfo*
hyscan_tile_color_info_new (void)
{
  HyScanTileColorInfo* info;
  guint32 colors[2], background, *cmap;
  guint length;

  /* Создаем структуру. */
  info = g_new0 (HyScanTileColorInfo, 1);

  /* Уровни по умолчанию. */
  info->black = 0.0;
  info->gamma = 1.0;
  info->white = 1.0;

  /* Цветовая схема по умолчанию. */
  background = hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0);
  colors[0] = hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 1.0);
  colors[1] = hyscan_tile_color_converter_d2i (0.0, 1.0, 1.0, 1.0);
  cmap = hyscan_tile_color_compose_colormap (colors, 2, &length);

  info->colormap = g_array_sized_new (FALSE, TRUE, sizeof (guint32), length);
  memcpy (info->colormap->data, cmap, length * sizeof (guint32));
  info->colormap->len = length;

  info->background = background;

  hyscan_tile_color_update_mnemonic (info);

  g_free (cmap);

  return info;
}

/* Функция ищет цветовую схему для источника. */
static HyScanTileColorInfo*
hyscan_tile_color_info_lookup (HyScanTileColorPrivate *priv,
                               HyScanSourceType        source)
{
  HyScanTileColorInfo *cmap;

  /* Ищем цветовую схему для запрошенного КД. */
  cmap = g_hash_table_lookup (priv->colorinfos, GINT_TO_POINTER (source));
  if (cmap != NULL)
    return cmap;

  /* Так как эта схема может быть не задана, ищем схему по умолчанию. */
  cmap = g_hash_table_lookup (priv->colorinfos, GINT_TO_POINTER (HYSCAN_SOURCE_INVALID));

  if (cmap != NULL)
    return cmap;

  /* Если ничего не получилось найти, так и скажем пользователю. */
  g_warning ("HyScanTileColor: failed to obtain fallback colormap");
  return NULL;
}

/* Функция освобождает HyScanTileColorInfo. */
static void
hyscan_tile_color_destroy_info (gpointer data)
{
  HyScanTileColorInfo *info = data;

  g_array_unref (info->colormap);
  g_free (info->mnemonic);
  g_free (info);
}

/* Функция устанавливает новые уровни. */
static gboolean
hyscan_tile_color_set_levels_internal (HyScanTileColor *color,
                                       HyScanSourceType source,
                                       gdouble          black,
                                       gdouble          gamma,
                                       gdouble          white)
{
  HyScanTileColorPrivate *priv = color->priv;
  HyScanTileColorInfo *link;

  if (black >= white ||
      black < 0.0 || black > 1.0 ||
      white < 0.0 || white > 1.0 ||
      gamma <= 0.0)
    {
      return FALSE;
    }

  g_mutex_lock (&priv->lock);

  /* Ищем в хэш-таблице. */
  link = g_hash_table_lookup (priv->colorinfos, GINT_TO_POINTER (source));

  /* Если не нашли, создаем новый. */
  if (link == NULL)
    {
      link = hyscan_tile_color_info_new ();
      g_hash_table_insert (priv->colorinfos, GINT_TO_POINTER (source), link);
    }

  /* Переписываем параметры. */
  link->black = black;
  link->gamma = gamma;
  link->white = white;

  /* Обновляем строку. */
  hyscan_tile_color_update_mnemonic (link);

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает цветовую схему. */
static gboolean
hyscan_tile_color_set_colormap_internal (HyScanTileColor *color,
                                         HyScanSourceType source,
                                         guint32         *colormap,
                                         guint            length,
                                         guint32          background)
{
  HyScanTileColorPrivate *priv = color->priv;
  HyScanTileColorInfo *link;

  if (colormap == NULL || length == 0)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем в хэш-таблице. */
  link = g_hash_table_lookup (priv->colorinfos, GINT_TO_POINTER (source));

  /* Если не нашли, создаем новый. */
  if (link == NULL)
    {
      link = hyscan_tile_color_info_new ();
      g_hash_table_insert (priv->colorinfos, GINT_TO_POINTER (source), link);
    }

  /* Освобождаем старый колормап. */
  if (link->colormap != NULL)
    g_array_unref (link->colormap);

  /* Выделяем память под новый, копируем, выставляем длину. */
  link->colormap = g_array_sized_new (FALSE, FALSE, sizeof (guint32), length);
  memcpy (link->colormap->data, colormap, length * sizeof (guint32));
  link->colormap->len = length;

  /* Цвет фона. */
  link->background = background;

  /* Обновляем строку. */
  hyscan_tile_color_update_mnemonic (link);

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция генерирует ключ для системы кэширования. */
static gchar*
hyscan_tile_color_cache_key (HyScanTile  *tile,
                             const gchar *mnemonic,
                             const gchar *path)
{
  gchar *key;
  key = g_strdup_printf ("color.%s.%s|%i.%i.%i.%i.%010.3f.%06.3f|%u.%i.%i.%i",
                                path, mnemonic,

                                tile->across_start,
                                tile->along_start,
                                tile->across_end,
                                tile->along_end,
                                tile->scale,
                                tile->ppi,

                                tile->upsample,
                                tile->flags,
                                tile->rotate,
                                tile->source);

  return key;
}

/* Функция составляет строковый хэш цветовой схемы. */
static void
hyscan_tile_color_update_mnemonic (HyScanTileColorInfo *cmap)
{
  guint32 hash = 0;

  /* Вычисляем хэш цветовой схемы. */
  if (cmap->colormap != NULL)
    hash = crc32 (0L, (gpointer)(cmap->colormap->data), cmap->colormap->len * sizeof (guint32));

  if (cmap->mnemonic != NULL)
    g_free (cmap->mnemonic);

  cmap->mnemonic = g_strdup_printf (".%f.%f.%f.%u.%u",
                                     cmap->black,
                                     cmap->gamma,
                                     cmap->white,
                                     hash,
                                     cmap->background);
}

/* Функция создает новый объект HyScanTileColor. */
HyScanTileColor*
hyscan_tile_color_new (HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_TILE_COLOR,
                       "cache", cache, NULL);
}

/* Функция открывает галс. */
void
hyscan_tile_color_open (HyScanTileColor *color,
                        const gchar     *db_uri,
                        const gchar     *project,
                        const gchar     *track)
{
  HyScanTileColorPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_COLOR (color));
  priv = color->priv;

  if (db_uri == NULL)
    db_uri = "none";
  if (project == NULL)
    project = "none";
  if (track == NULL)
    track = "none";

  g_mutex_lock (&priv->lock);

  g_clear_pointer (&priv->path, g_free);
  priv->path = g_strdup_printf ("%s.%s.%s", db_uri, project, track);

  g_mutex_unlock (&priv->lock);
}

/* Функция закрывает галс. */
void
hyscan_tile_color_close (HyScanTileColor *color)
{
  HyScanTileColorPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_COLOR (color));
  priv = color->priv;

  g_clear_pointer (&priv->path, g_free);
}

/* Функция ищет тайл в кэше. */
gboolean
hyscan_tile_color_check (HyScanTileColor *color,
                         HyScanTile      *requested_tile,
                         HyScanTile      *cached_tile)
{
  HyScanTileColorCache header = {.magic = 0};
  HyScanTileColorPrivate *priv;
  HyScanTileColorInfo *cmap;
  gchar *key;
  gboolean found;

  guint32 size = sizeof (HyScanTile);

  g_return_val_if_fail (HYSCAN_IS_TILE_COLOR (color), FALSE);
  priv = color->priv;

  if (requested_tile == NULL ||  cached_tile == NULL)
    return FALSE;

  if (priv->cache == NULL)
    return FALSE;

  /* Cоставляем ключ из тайла, цветовой схемы и пути. */
  g_mutex_lock (&priv->lock);
  cmap = hyscan_tile_color_info_lookup (priv, requested_tile->source);

  if (cmap == NULL)
    {
      g_mutex_unlock (&priv->lock);
      return FALSE;
    }

  key = hyscan_tile_color_cache_key (requested_tile, cmap->mnemonic, priv->path);
  g_mutex_unlock (&priv->lock);

  /* Ищем тайл в кэше. */
  hyscan_buffer_wrap_data (priv->read1, HYSCAN_DATA_BLOB,
                           cached_tile, size);


  //// found = hyscan_cache_get (priv->cache, key, NULL, cached_tile, &size);
  //hyscan_buffer_wrap_data (priv->read1, HYSCAN_DATA_BLOB, cached_tile, size);
  hyscan_buffer_wrap_data (priv->read1, HYSCAN_DATA_BLOB, &header, sizeof (header));
  found = hyscan_cache_get2 (priv->cache, key, NULL, size, priv->read1, NULL);
  g_free (key);

  if (!found || header.magic != TILE_COLOR_MAGIC)
    return FALSE;

  *cached_tile = header.tile;
  return TRUE;
}

/* Функция забирает тайл из кэша. */
gboolean
hyscan_tile_color_get (HyScanTileColor   *color,
                       HyScanTile        *requested_tile,
                       HyScanTile        *cached_tile,
                       HyScanTileSurface *surface)
{
  HyScanTileColorCache header = {.magic = 0};
  HyScanTileColorPrivate *priv;
  gchar *key;
  HyScanTileColorInfo *cmap;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TILE_COLOR (color), FALSE);
  priv = color->priv;

  if (requested_tile == NULL || cached_tile == NULL || surface == NULL)
    return FALSE;

  if (priv->cache == NULL)
    return FALSE;

  /* Cоставляем ключ из тайла, цветовой схемы и пути. */
  g_mutex_lock (&priv->lock);
  cmap = hyscan_tile_color_info_lookup (priv, requested_tile->source);

  if (cmap == NULL)
    {
      g_mutex_unlock (&priv->lock);
      return FALSE;
    }

  key = hyscan_tile_color_cache_key (requested_tile, cmap->mnemonic, priv->path);
  g_mutex_unlock (&priv->lock);

  /* Ищем тайл в кэше. */
  hyscan_buffer_wrap_data (priv->read1, HYSCAN_DATA_BLOB,
                           &header, sizeof (header));
  hyscan_buffer_wrap_data (priv->read2, HYSCAN_DATA_BLOB,
                           surface->data, G_MAXUINT32);

  status = hyscan_cache_get2 (priv->cache, key, NULL, sizeof (header),
                              priv->read1, priv->read2);

  /* Проверяем магическую константу. */
  g_free (key);

  if (!status || header.magic != TILE_COLOR_MAGIC)
    return FALSE;

  *cached_tile = header.tile;
  return status;
}

/* Функция раскрашивает и кладет в кэш новый тайл. */
void
hyscan_tile_color_add (HyScanTileColor   *color,
                       HyScanTile        *tile,
                       gfloat            *input,
                       gint               size,
                       HyScanTileSurface *surface)
{
  gfloat   black, gamma, white, levels;
  GArray  *colormap;
  guint32 *colormap_data;
  guint32  background;
  HyScanTileColorInfo *cmap;

  gchar *key, *data;
  gint i, j, width, height, stride;
  HyScanTileColorPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_COLOR (color));
  if (input == NULL || tile == NULL || surface == NULL)
    return;

  data       = surface->data;
  width      = surface->width;
  height     = surface->height;
  stride     = surface->stride;
  priv       = color->priv;

  /* Ищем цветовую схему для КД, указанного в тайле и составляем ключ. */
  g_mutex_lock (&priv->lock);
  cmap = hyscan_tile_color_info_lookup (priv, tile->source);

  if (cmap == NULL)
    return;

  black = cmap->black;
  gamma = cmap->gamma;
  white = cmap->white;

  colormap = g_array_ref (cmap->colormap);
  colormap_data = (guint32*)(colormap->data);
  levels = colormap->len;

  background = cmap->background;

  if (priv->cache != NULL)
    key = hyscan_tile_color_cache_key (tile, cmap->mnemonic, priv->path);
  else
    key = NULL;

  g_mutex_unlock (&priv->lock);

  /* Уровни и цвет. */
  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
        {
          gfloat point = input[i * width + j];

          /* Преобразование уровней. */
          if (point >= white)
            point = 1.0;
          else if (point <= black && point >= 0.0)
            point = 0.0;
          else if (point >= 0)
            point = pow((point - black) / (white - black), gamma);

          /* Складываем в выходной массив. */
          if (G_LIKELY (point >= 0))
            HYSCAN_COLOR_AT (i, j) = colormap_data[(gint)(point * (levels - 1))];
          else
            HYSCAN_COLOR_AT (i, j) = background;
        }
   }

   /* Включение этого флага позволяет отобразить границы тайлов. */
#ifdef HYSCAN_CORE_TILE_BORDERS
# warning HYSCAN_CORE_TILE_BORDERS is defined. I will draw tile borders.
  for (i = 0; i < 10; i++)
    {
      HYSCAN_COLOR_AT (1, i + 1) = 0xFFFF0000;
      HYSCAN_COLOR_AT (i + 1, 1) = 0xFFFF0000;
      HYSCAN_COLOR_AT (height - 2, width - 2 - i) = (tile->finalized) ? 0xFF00FF00 : 0xFF0000FF;
      HYSCAN_COLOR_AT (height - 2 - i, width - 2) = (tile->finalized) ? 0xFF00FF00 : 0xFF0000FF;
    }
#endif

  /* Теперь складываем в кэш. */
  if (priv->cache != NULL)
    {
      HyScanTileColorCache header;
      guint32 size1 = sizeof (HyScanTileColorCache);
      guint32 size2 = height * stride;

      header.magic = TILE_COLOR_MAGIC;
      header.size = size1 + size2;
      header.tile = *tile;

      g_mutex_lock (&priv->lock);

      hyscan_buffer_wrap_data (priv->write1, HYSCAN_DATA_BLOB, &header, size1);
      hyscan_buffer_wrap_data (priv->write2, HYSCAN_DATA_BLOB, data, size2);
      hyscan_cache_set2 (priv->cache, key, NULL, priv->write1, priv->write2);

      g_mutex_unlock (&priv->lock);
    }

  g_free (key);
  g_array_unref (colormap);
}

/* Функция устанавливает новые уровни. */
gboolean
hyscan_tile_color_set_levels (HyScanTileColor *color,
                              HyScanSourceType source,
                              gdouble          black,
                              gdouble          gamma,
                              gdouble          white)
{
  g_return_val_if_fail (HYSCAN_IS_TILE_COLOR (color), FALSE);

  return hyscan_tile_color_set_levels_internal (color, source,
                                                black, gamma, white);
}

/* Функция устанавливает резервные уровни. */
gboolean
hyscan_tile_color_set_levels_for_all (HyScanTileColor *color,
                                      gdouble          black,
                                      gdouble          gamma,
                                      gdouble          white)
{
  GHashTableIter iter;
  gpointer key, value;
  gboolean result = TRUE;

  g_return_val_if_fail (HYSCAN_IS_TILE_COLOR (color), FALSE);

  g_hash_table_iter_init (&iter, color->priv->colorinfos);
  while (g_hash_table_iter_next (&iter, &key, &value))
    result &= hyscan_tile_color_set_levels_internal (color, (HyScanSourceType) key, black, gamma, white);

  return result;
}

/* Функция устанавливает цветовую схему. */
gboolean
hyscan_tile_color_set_colormap (HyScanTileColor *color,
                                HyScanSourceType source,
                                guint32         *colormap,
                                guint            length,
                                guint32          background)
{
  g_return_val_if_fail (HYSCAN_IS_TILE_COLOR (color), FALSE);

  return hyscan_tile_color_set_colormap_internal (color, source, colormap, length, background);
}

/* Функция устанавливает резервную цветовую схему. */
gboolean
hyscan_tile_color_set_colormap_for_all (HyScanTileColor *color,
                                        guint32         *colormap,
                                        guint            length,
                                        guint32          background)
{
  GHashTableIter iter;
  gpointer key, value;
  gboolean result = TRUE;

  g_return_val_if_fail (HYSCAN_IS_TILE_COLOR (color), FALSE);

  g_hash_table_iter_init (&iter, color->priv->colorinfos);
  while (g_hash_table_iter_next (&iter, &key, &value))
    result &= hyscan_tile_color_set_colormap_internal (color, (HyScanSourceType) key, colormap, length, background);

  return result;
}

/* Функция составляет цветовую схему. */
guint32*
hyscan_tile_color_compose_colormap (guint32 *colors,
                                    guint    num,
                                    guint   *length)
{
  guint32 *out;
  guint len = 256;

  gdouble r0 = 0, g0 = 0, b0 = 0, a0 = 0;
  gdouble r1 = 0, g1 = 0, b1 = 0, a1 = 0;
  guint i, j;
  gdouble step;
  gdouble pos, prev_pos;

  /* Если передано 0 элементов, выходим. */
  if (num == 0 || num > 256)
    return NULL;

  if (num == 256)
    {
      out = g_memdup (colors, len * sizeof (guint32));
      goto exit;
    }

  out = g_malloc0 (len * sizeof (guint32));

  out[0] = colors[0];

  step = 255.0 / (num - 1);
  for (i = 1, prev_pos = 0; i < num; i++)
    {
      pos = round (i * step);

      out[(gint)pos] = colors[i];
      a0 = (colors[i-1] & 0xFF000000) >> 24;
      r0 = (colors[i-1] & 0x00FF0000) >> 16;
      g0 = (colors[i-1] & 0x0000FF00) >> 8;
      b0 = (colors[i-1] & 0x000000FF) >> 0;
      a1 = (colors[i] & 0xFF000000) >> 24;
      r1 = (colors[i] & 0x00FF0000) >> 16;
      g1 = (colors[i] & 0x0000FF00) >> 8;
      b1 = (colors[i] & 0x000000FF) >> 0;

      for (j = prev_pos + 1; j < pos; j++)
        {
          guint32 r, g, b, a;
          b = b0 + (b1 - b0) * j / (pos - prev_pos);
          g = g0 + (g1 - g0) * j / (pos - prev_pos);
          r = r0 + (r1 - r0) * j / (pos - prev_pos);
          a = a0 + (a1 - a0) * j / (pos - prev_pos);

          out[j] = (gint)(a << 24) | (gint)(r << 16) | (gint)(g << 8) | (gint)(b);
        }
      prev_pos = pos;
    }

exit:
  if (length != NULL)
    *length = len;
  return out;
}

/* Функция переводит значение цвета из отдельных компонентов в упакованное 32-х битное значение. */
guint32
hyscan_tile_color_converter_d2i (gdouble red,
                                 gdouble green,
                                 gdouble blue,
                                 gdouble alpha)
{
  guint32 ired, igreen, iblue, ialpha;
  guint32 color;

  red = CLAMP (red, 0.0, 1.0);
  green = CLAMP (green, 0.0, 1.0);
  blue = CLAMP (blue, 0.0, 1.0);
  alpha = CLAMP (alpha, 0.0, 1.0);

  ired = red * 0xFF;
  igreen = green * 0xFF;
  iblue = blue * 0xFF;
  ialpha = alpha * 0xFF;

  color = (ialpha << 24) | (ired << 16) | (igreen << 8) | (iblue);
  return color;
}

/* Функция разбирает 32-х битное значение цвета по слоям. */
void
hyscan_tile_color_converter_i2d (guint32  color,
                                 gdouble *red,
                                 gdouble *green,
                                 gdouble *blue,
                                 gdouble *alpha)
{
  if (blue != NULL)
    *blue  = ((color >>  0) & 0x000000FF) / 255.0;
  if (green != NULL)
    *green = ((color >>  8) & 0x000000FF) / 255.0;
  if (red != NULL)
    *red   = ((color >> 16) & 0x000000FF) / 255.0;
  if (alpha != NULL)
    *alpha = ((color >> 24) & 0x000000FF) / 255.0;
}

/* Функция упаковывает целочисленные цвета в 32-х битное значение. */
guint32
hyscan_tile_color_converter_c2i (guchar red,
                                 guchar green,
                                 guchar blue,
                                 guchar alpha)
{
  guint32 color;
  guint32 ired = red;
  guint32 igreen = green;
  guint32 iblue = blue;
  guint32 ialpha = alpha;

  color = (ialpha << 24) | (ired << 16) | (igreen << 8) | (iblue);

  return color;
}

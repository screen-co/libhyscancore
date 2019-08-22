#include <hyscan-cached.h>
#include <hyscan-tile-color.h>
#include <string.h>
#include <glib/gprintf.h>

typedef struct
{
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble alpha;
} TestColor;

static TestColor make_color (gdouble   red,
                             gdouble   green,
                             gdouble   blue,
                             gdouble   alpha);
static gboolean  colors_equal  (TestColor color1,
                                TestColor color2);

static gboolean  conversion_test (void);

int
main (int argc, char **argv)
{
  HyScanCache     *cache;
  HyScanTileColor *color;

  HyScanTile *tile = NULL;
  HyScanTileCacheable cacheable;
  HyScanTileSurface surface;
  gfloat *data = NULL;
  gint size, i, pixels;

  guint32 *cmap, colors[2];
  guint32 base;
  guint cmap_len;

  /* Запись данных. */
  gboolean status = TRUE;
  gboolean passed = FALSE;

  cache = HYSCAN_CACHE (hyscan_cached_new (512));
  color = hyscan_tile_color_new (cache);

  /* Сначала потестируем вспомогательные функции. */
  if (!conversion_test ())
    goto exit;

  hyscan_tile_color_open (color, "db", "project", "track");

  tile = hyscan_tile_new (NULL);
  tile->info.across_start = 0;
  tile->info.along_start = 0;
  tile->info.across_end = 10;
  tile->info.along_end = 10;
  tile->info.scale = 1;
  tile->info.ppi = 1;
  tile->info.upsample = 1;
  tile->info.flags = 0;
  tile->info.rotate = 1;
  tile->info.source = 1;
  tile->cacheable.w = 10;
  tile->cacheable.h = 10;
  tile->cacheable.finalized = 1;

  /* Подготавливаем tilesurface. */
  surface.width = 10;
  surface.height = 10;
  surface.stride = 10 * sizeof (guint32);
  surface.data = g_malloc0 (surface.height * surface.stride);

  /* Подготавливаем данные. */
  pixels = surface.width * surface.height;
  size = pixels * sizeof (gfloat);
  data = g_malloc0 (size);

  for (i = 0; i < pixels; i++)
    data[i] = (gfloat)i / pixels;

  /* Разукрасим тайл цветовой схемой, состоящей только из нулей. */
  base = colors[1] = colors[0] = hyscan_tile_color_converter_d2i (1.0, 0.5, 0.25, 0.0);
  cmap = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);
  hyscan_tile_color_set_colormap_for_all (color, cmap, cmap_len, base);
  g_free (cmap);

  hyscan_tile_color_add (color, tile, data, size, &surface);

  for (i = 0; i < pixels; i++)
    {
      guint32 val;
      val = ((guint32*)(surface.data))[i];
      if (val != base)
        {
          g_print ("colorisation fail at line %i\n", __LINE__);
          g_print ("Expected %x, got %x\n", base, val);
          goto exit;
        }
    }

  /* Разукрасим тайл цветовой схемой, состоящей только из 0xFFFFFFFF. */
  base = colors[1] = colors[0] = hyscan_tile_color_converter_d2i (0.25, 1.0, 0.5, 1.0);
  cmap = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);
  hyscan_tile_color_set_colormap_for_all (color, cmap, cmap_len, base);
  g_free (cmap);

  hyscan_tile_color_add (color, tile, data, size, &surface);

  for (i = 0; i < pixels; i++)
    {
      guint32 val;
      val = ((guint32*)(surface.data))[i];
      if (val != base)
        {
          g_print ("Colorisation fail at line %i\n", __LINE__);
          g_print ("Expected %x, got %x\n", base, val);
          goto exit;
        }
    }

  hyscan_tile_color_add (color, tile, data, size, &surface);

  status = hyscan_tile_color_check (color, tile, &cacheable);
  if (!status)
    {
      g_print ("Tile not found in cache (though expected)\n");
      goto exit;
    }

  /* Теперь считаем то, что лежит в кэше. */
  status = hyscan_tile_color_get (color, tile, &cacheable, &surface);
  if (!status)
    {
      g_print ("Tile not found in cache (though expected)\n");
      goto exit;
    }
  for (i = 0; i < pixels; i++)
    {
      guint32 val;
      val = ((guint32*)(surface.data))[i];
      if (val != base)
        {
          g_print ("In tile from cache:\n");
          g_print ("Сolorisation fail at line %i\n", __LINE__);
          g_print ("Expected %x, got %x\n", base, val);
          goto exit;
        }
    }
  passed = TRUE;

exit:
  hyscan_tile_color_close (color);

  g_free (surface.data);
  g_free (data);

  g_clear_object (&tile);
  g_clear_object (&cache);
  g_clear_object (&color);

  g_print ("test %s\n", passed ? "passed" : "failed");
  return passed ? 0 : 1;
}


static TestColor
make_color (gdouble red,
            gdouble green,
            gdouble blue,
            gdouble alpha)
{
  TestColor retval;

  retval.red = red;
  retval.green = green;
  retval.blue = blue;
  retval.alpha = alpha;

  return retval;
}

static gboolean
colors_equal (TestColor color1,
           TestColor color2)
{
  return (color1.red == color2.red && color1.green == color2.green &&
          color1.blue == color2.blue && color1.alpha == color2.alpha);
}

static gboolean
conversion_test (void)
{
  TestColor col1, col2;
  guint32 tmp;

  col1 = make_color (0.0, 1.0, 0.0, 1.0);
  tmp = hyscan_tile_color_converter_d2i (col1.red, col1.green, col1.blue, col1.alpha);
  hyscan_tile_color_converter_i2d (tmp, &col2.red, &col2.green, &col2.blue, &col2.alpha);
  if (!colors_equal (col1, col2))
    {
      g_print ("Color convertion fail");
      return FALSE;
    }
  col1 = make_color (1.0, 0.0, 1.0, 0.0);
  tmp = hyscan_tile_color_converter_d2i (col1.red, col1.green, col1.blue, col1.alpha);
  hyscan_tile_color_converter_i2d (tmp, &col2.red, &col2.green, &col2.blue, &col2.alpha);
  if (!colors_equal (col1, col2))
    {
      g_print ("Color convertion fail");
      return FALSE;
    }

  return TRUE;
}

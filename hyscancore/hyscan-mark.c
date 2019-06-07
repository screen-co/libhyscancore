#include "hyscan-mark.h"

G_DEFINE_BOXED_TYPE (HyScanMark, hyscan_mark,
                     hyscan_mark_copy, hyscan_mark_free)

HyScanMark*
hyscan_mark_new (void)
{
  HyScanMark* mark;
  mark = g_slice_new0 (HyScanMark);
  return mark;
}

HyScanMark*
hyscan_mark_copy (HyScanMark *mark)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;
  HyScanMark *copy;

  if (mark == NULL)
    return NULL;

  copy = hyscan_mark_new ();

  switch (mark->type)
    {
    case HYSCAN_MARK_WATERFALL:
      hyscan_mark_waterfall_set_track (&copy->waterfall, mark->waterfall.track);
      hyscan_mark_waterfall_set_center (&copy->waterfall, mark->waterfall.source0,
                                        mark->waterfall.index0, mark->waterfall.count0);
      break;

    case HYSCAN_MARK_GEO:
      hyscan_mark_geo_set_center (&copy->geo, mark->geo.center);
      break;

    default:
      g_return_val_if_reached (NULL);
    }

  /* Копируем общие поля. */
  hyscan_mark_set_text (copy, any->name,
                              any->description,
                              any->operator_name);
  hyscan_mark_set_labels (copy, any->labels);
  hyscan_mark_set_ctime (copy, any->creation_time);
  hyscan_mark_set_mtime (copy, any->modification_time);
  hyscan_mark_set_size (copy, any->width, any->height);

  return copy;
}


void
hyscan_mark_free (HyScanMark *mark)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  if (mark == NULL)
    return;

  /* Освобождаем общие поля. */
  g_free (any->name);
  g_free (any->description);
  g_free (any->operator_name);

  if (mark->type == HYSCAN_MARK_WATERFALL)
    g_free (mark->waterfall.track);

  g_slice_free (HyScanMark, mark);
}


void
hyscan_mark_waterfall_set_track (HyScanMarkWaterfall *mark,
                                 const gchar         *track)
{
  g_free (mark->track);
  mark->track = g_strdup (track);
}

void
hyscan_mark_set_text (HyScanMark  *mark,
                      const gchar *name,
                      const gchar *description,
                      const gchar *oper)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  g_free (any->name);
  g_free (any->description);
  g_free (any->operator_name);

  any->name = g_strdup (name);
  any->description = g_strdup (description);
  any->operator_name = g_strdup (oper);

}

void
hyscan_mark_set_labels (HyScanMark *mark,
                        guint64     labels)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->labels = labels;
}

void
hyscan_mark_set_ctime (HyScanMark   *mark,
                       gint64        creation)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->creation_time = creation;
}

void
hyscan_mark_set_mtime (HyScanMark   *mark,
                       gint64        modification)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->modification_time = modification;
}

void
hyscan_mark_waterfall_set_center (HyScanMarkWaterfall *mark,
                                  HyScanSourceType     source,
                                  guint32              index,
                                  guint32              count)
{
  mark->source0 = source;
  mark->index0 = index;
  mark->count0 = count;
}

void
hyscan_mark_set_size (HyScanMark *mark,
                      guint32     width,
                      guint32     height)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->width = width;
  any->height = height;
}

void
hyscan_mark_geo_set_center (HyScanMarkGeo     *mark,
                            HyScanGeoGeodetic  center)
{
  mark->center = center;
}

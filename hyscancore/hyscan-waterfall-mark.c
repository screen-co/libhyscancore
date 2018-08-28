#include "hyscan-waterfall-mark.h"

G_DEFINE_BOXED_TYPE (HyScanWaterfallMark, hyscan_waterfall_mark,
                     hyscan_waterfall_mark_copy, hyscan_waterfall_mark_free)

HyScanWaterfallMark*
hyscan_waterfall_mark_new (void)
{
  HyScanWaterfallMark* mark;
  mark = g_slice_new0 (HyScanWaterfallMark);
  return mark;
}


HyScanWaterfallMark*
hyscan_waterfall_mark_copy (HyScanWaterfallMark *mark)
{
  HyScanWaterfallMark* copy;

  if (mark == NULL)
    return NULL;

  copy = hyscan_waterfall_mark_new ();

  hyscan_waterfall_mark_set_track (copy, mark->track);
  hyscan_waterfall_mark_set_text (copy, mark->name,
                                  mark->description,
                                  mark->operator_name);
  hyscan_waterfall_mark_set_labels (copy, mark->labels);
  hyscan_waterfall_mark_set_ctime (copy, mark->creation_time);
  hyscan_waterfall_mark_set_mtime (copy, mark->modification_time);
  hyscan_waterfall_mark_set_center (copy, mark->source0,
                                    mark->index0, mark->count0);
  hyscan_waterfall_mark_set_size (copy, mark->width, mark->height);

  return copy;
}


void
hyscan_waterfall_mark_free (HyScanWaterfallMark *mark)
{
  if (mark == NULL)
    return;

  g_free (mark->track);
  g_free (mark->name);
  g_free (mark->description);
  g_free (mark->operator_name);

  g_slice_free (HyScanWaterfallMark, mark);
}


void
hyscan_waterfall_mark_set_track (HyScanWaterfallMark *mark,
                                 const gchar         *track)
{
  g_free (mark->track);
  mark->track = g_strdup (track);
}

void
hyscan_waterfall_mark_set_text (HyScanWaterfallMark *mark,
                                const gchar         *name,
                                const gchar         *description,
                                const gchar         *oper)
{
  g_free (mark->name);
  g_free (mark->description);
  g_free (mark->operator_name);

  mark->name = g_strdup (name);
  mark->description = g_strdup (description);
  mark->operator_name = g_strdup (oper);

}

void
hyscan_waterfall_mark_set_labels (HyScanWaterfallMark *mark,
                                  guint64              labels)
{
  mark->labels = labels;
}

void
hyscan_waterfall_mark_set_ctime (HyScanWaterfallMark   *mark,
                                 gint64                 creation)
{
  mark->creation_time = creation;
}

void
hyscan_waterfall_mark_set_mtime (HyScanWaterfallMark   *mark,
                                 gint64                 modification)
{
  mark->modification_time = modification;
}

void
hyscan_waterfall_mark_set_center (HyScanWaterfallMark *mark,
                                  HyScanSourceType     source,
                                  guint32              index,
                                  guint32              count)
{
  mark->source0 = source;
  mark->index0 = index;
  mark->count0 = count;
}

void
hyscan_waterfall_mark_set_size (HyScanWaterfallMark *mark,
                                guint32              width,
                                guint32              height)
{
  mark->width = width;
  mark->height = height;
}

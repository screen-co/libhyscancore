/*
 * \file hyscan-tile-common.c
 *
 * \brief Исходный код вспомогательных функций для тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include <hyscan-waterfall-mark.h>

void
hyscan_waterfall_mark_free (HyScanWaterfallMark *mark)
{
  if (mark == NULL)
    return;

  /* Освобождаем внутренние поля. */
  g_free (mark->name);
  g_free (mark->description);
  g_free (mark->operator_name);
}

void
hyscan_waterfall_mark_deep_free (HyScanWaterfallMark *mark)
{
  if (mark == NULL)
    return;

  hyscan_waterfall_mark_free (mark);
  g_free (mark);
}

HyScanWaterfallMark*
hyscan_waterfall_mark_copy (const HyScanWaterfallMark *mark)
{
  HyScanWaterfallMark *new;

  if (mark == NULL)
    return NULL;

  new = g_new (HyScanWaterfallMark, 1);

  new->name              = g_strdup (mark->name);
  new->description       = g_strdup (mark->description);
  new->operator_name     = g_strdup (mark->operator_name);
  new->labels            = mark->labels;
  new->creation_time     = mark->creation_time;
  new->modification_time = mark->modification_time;
  new->source0           = mark->source0;
  new->index0            = mark->index0;
  new->count0            = mark->count0;
  new->source1           = mark->source1;
  new->index1            = mark->index1;
  new->count1            = mark->count1;
  
  return new;
}

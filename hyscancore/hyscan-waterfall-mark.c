/*
 * \file hyscan-waterfall-mark.c
 *
 * \brief Исходный код вспомогательных функций для меток.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include <hyscan-waterfall-mark.h>

/* Функция освобождает внутренние поля структуры. */
void
hyscan_waterfall_mark_free (HyScanWaterfallMark *mark)
{
  if (mark == NULL)
    return;

  g_free (mark->track);
  g_free (mark->name);
  g_free (mark->description);
  g_free (mark->operator_name);
}

/* Функция полностью освобождает память, занятую меткой. */
void
hyscan_waterfall_mark_deep_free (HyScanWaterfallMark *mark)
{
  if (mark == NULL)
    return;

  hyscan_waterfall_mark_free (mark);
  g_free (mark);
}

/* Функция возвращает полную копию метки. */
HyScanWaterfallMark*
hyscan_waterfall_mark_copy (const HyScanWaterfallMark *_mark)
{
  HyScanWaterfallMark *mark;

  g_return_val_if_fail (_mark != NULL, NULL);

  mark = g_new (HyScanWaterfallMark, 1);

  mark->track             = g_strdup (_mark->track);
  mark->name              = g_strdup (_mark->name);
  mark->description       = g_strdup (_mark->description);
  mark->operator_name     = g_strdup (_mark->operator_name);
  mark->labels            = _mark->labels;
  mark->creation_time     = _mark->creation_time;
  mark->modification_time = _mark->modification_time;
  mark->source0           = _mark->source0;
  mark->index0            = _mark->index0;
  mark->count0            = _mark->count0;
  mark->width             = _mark->width;
  mark->height            = _mark->height;

  return mark;
}

/**
 *
 * \file hyscan-waterfall-mark.h
 *
 * \brief Структуры и вспомогательные функции для меток водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanWaterfallMark HyScanWaterfallMark - Структуры  и вспомогательные
 * функции для тайлов.
 *
 * Метки привязываются к каналу данных, конкретной строке и отсчету.
 *
 * Здесь описывается структура \link HyScanWaterfallMark \endlink, в которой
 * хранятся параметры меток. Помимо это представлены функции для копирования,
 * и освобождения меток.
 *
 */

#ifndef __HYSCAN_WATERFALL_MARK_H__
#define __HYSCAN_WATERFALL_MARK_H__

#include <hyscan-core-types.h>

/** \brief Структура с информацией о метке. */
typedef struct
{
  gchar            *track;             /**< Идентификатор галса. */
  gchar            *name;              /**< Название метки. */
  gchar            *description;       /**< Описание. */
  gchar            *operator_name;     /**< Имя оператора. */
  guint64           labels;            /**< Метки. */
  gint64            creation_time;     /**< Время создания. */
  gint64            modification_time; /**< Время последней модификации. */
  HyScanSourceType  source0;           /**< Источник данных. */
  guint32           index0;            /**< Индекс данных. */
  guint32           count0;            /**< Отсчёт в строке. */
  guint32           width;             /**< Ширина. */
  guint32           height;            /**< Высота. */
} HyScanWaterfallMark;

/**
 *
 * Функция освобождает внутренние поля структуры.
 *
 * \param mark указатель на метку.
 *
 */
HYSCAN_API
void                 hyscan_waterfall_mark_free         (HyScanWaterfallMark       *mark);

/**
 *
 * Функция полностью освобождает память, занятую меткой.
 *
 * \param mark указатель на метку.
 */
HYSCAN_API
void                 hyscan_waterfall_mark_deep_free    (HyScanWaterfallMark       *mark);

/**
 *
 * Функция возвращает полную копию метки.
 *
 * \param mark указатель на метку.
 *
 * \return указатель на копию метки. Эту память требуется освободить.
 */
HYSCAN_API
HyScanWaterfallMark *hyscan_waterfall_mark_copy         (const HyScanWaterfallMark *mark);

#endif /* __HYSCAN_WATERFALL_MARK_H__ */

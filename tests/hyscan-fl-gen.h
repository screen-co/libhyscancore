/*
 * \file hyscan-fl-gen.h
 *
 * \brief Заголовочный файл класса генерации тестовых данных вперёдсмотрящего локатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * Класс предназначен для записи тестовых данных вперёдсмотрящего локатора.
 *
 * Тестовые данные представляют собой отражение от целей по всей дистанции
 * приёма с изменением направления от крайнего левого до крайнего правого.
 *
 */

#ifndef __HYSCAN_FL_GEN_H__
#define __HYSCAN_FL_GEN_H__

#include <hyscan-forward-look-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_FL_GEN             (hyscan_fl_gen_get_type ())
#define HYSCAN_FL_GEN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_FL_GEN, HyScanFLGen))
#define HYSCAN_IS_FL_GEN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_FL_GEN))
#define HYSCAN_FL_GEN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_FL_GEN, HyScanFLGenClass))
#define HYSCAN_IS_FL_GEN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_FL_GEN))
#define HYSCAN_FL_GEN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_FL_GEN, HyScanFLGenClass))

typedef struct _HyScanFLGen HyScanFLGen;
typedef struct _HyScanFLGenPrivate HyScanFLGenPrivate;
typedef struct _HyScanFLGenClass HyScanFLGenClass;

struct _HyScanFLGen
{
  GObject parent_instance;

  HyScanFLGenPrivate *priv;
};

struct _HyScanFLGenClass
{
  GObjectClass parent_class;
};

GType                  hyscan_fl_gen_get_type          (void);

/* Функция создаёт новый объект HyScanFLGen. */
HyScanFLGen           *hyscan_fl_gen_new               (void);

/* Функция задаёт местоположение приёмной антенны локатора. */
void                   hyscan_fl_gen_set_position      (HyScanFLGen                   *fl_gen,
                                                        HyScanAntennaPosition         *position);

/* Функция задаёт параметры локатора. */
void                   hyscan_fl_gen_set_info          (HyScanFLGen                   *fl_gen,
                                                        HyScanRawDataInfo             *info);

/* Функция задаёт проект и галс в который ведётся запись. */
gboolean               hyscan_fl_gen_set_track         (HyScanFLGen                   *fl_gen,
                                                        HyScanDB                      *db,
                                                        const gchar                   *project_name,
                                                        const gchar                   *track_name);

/* Функция генерирует и записывает в галс одну строку данных. */
gboolean               hyscan_fl_gen_generate          (HyScanFLGen                   *fl_gen,
                                                        guint32                        n_points,
                                                        gint64                         time);

/* Функция проверяет валидность данных. */
gboolean               hyscan_fl_gen_check             (const HyScanForwardLookDOA    *doa,
                                                        guint32                        n_points,
                                                        gint64                         time,
                                                        gdouble                        alpha);


G_END_DECLS

#endif /* __HYSCAN_FL_GEN_H__ */

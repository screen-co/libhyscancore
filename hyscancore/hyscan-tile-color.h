/**
 *
 * \file hyscan-tile-color.h
 *
 * \brief Заголовочный файл класса раскрашивания тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTileColor HyScanTileColor - раскрашивание тайлов.
 *
 * Данный класс предназначен для преобразования тайлов, состоящих из чисел с плавающей
 * точкой в диапазоне [0.0; 1.0] в целочисленные, где каждой точке исходного тайла
 * соответствуют 4 целочисленных значения (RGBA) в диапазоне [0; 255].
 *
 * Цветовая схема представляет собой массив элементов типа guint32.
 *
 * Преобразование идет в 2 этапа:
 * 1. Белая точка, черная точка и гамма-коррекция.
 * 2. Преобразование чисел с плавающей точкой в целочисленные с учетом цветовой схемы.
 *
 * Базовые методы:
 * - #hyscan_tile_color_new - создание нового объекта \link HyScanTileColor \endlink;
 * - #hyscan_tile_color_set_cache - установка системы кэширования;
 * - #hyscan_tile_color_open - установка БД, проекта и галса;
 * - #hyscan_tile_color_close - закрытие БД, проекта и галса;
 * - #hyscan_tile_color_check - поиск тайла в кэше;
 * - #hyscan_tile_color_get - получение тайла из кэша;
 * - #hyscan_tile_color_add - добавление нового тайла;
 * - #hyscan_tile_color_set_levels - установка уровней (черная/белая точка, гамма-коррекция);
 * - #hyscan_tile_color_set_colormap - установка цветовой схемы.
 *
 * Вспомогательные методы для работы с цветами:
 * - #hyscan_tile_color_compose_colormap - составляет цветовую схему;
 * - #hyscan_tile_color_converter_d2i - упаковка четырех составляющих цвета в 32-битное значение;
 * - #hyscan_tile_color_converter_i2d - обратная операция.
 *
 * Потокобезопасность.
 *
 * Функции #hyscan_tile_color_set_cache, #hyscan_tile_color_open, #hyscan_tile_color_close,
 * #hyscan_tile_color_add - потокобезопасны. Их можно вызывать из любого потока, но ровно
 * до тех пор, пока не вызывается любая другая функция. Все остальные функции
 * можно вызывать только тогда, когда hyscan_tile_color_add не вызвана.
 * Предполагается также, что все функции вызываются из основного потока.
 */

#ifndef __HYSCAN_TILE_COLOR_H__
#define __HYSCAN_TILE_COLOR_H__

#include <hyscan-cache.h>
#include <hyscan-tile-common.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TILE_COLOR             (hyscan_tile_color_get_type ())
#define HYSCAN_TILE_COLOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TILE_COLOR, HyScanTileColor))
#define HYSCAN_IS_TILE_COLOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TILE_COLOR))
#define HYSCAN_TILE_COLOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TILE_COLOR, HyScanTileColorClass))
#define HYSCAN_IS_TILE_COLOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TILE_COLOR))
#define HYSCAN_TILE_COLOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TILE_COLOR, HyScanTileColorClass))

typedef struct _HyScanTileColor HyScanTileColor;
typedef struct _HyScanTileColorPrivate HyScanTileColorPrivate;
typedef struct _HyScanTileColorClass HyScanTileColorClass;

/** \brief Структура с байтовым представлением раскрашенного тайла. */
typedef struct
{
  gint                 width;  /**< Ширина. */
  gint                 height; /**< Высота. */
  gint                 stride; /**< Шаг, ширина одной строки в байтах. */
  gpointer             data;   /**< Указатель на область данных для отрисовки. */
} HyScanTileSurface;

struct _HyScanTileColor
{
  GObject parent_instance;

  HyScanTileColorPrivate *priv;
};

struct _HyScanTileColorClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_tile_color_get_type             (void);

/**
 *
 * Функция создает новый объект \link HyScanTileColor \endlink
 * \param cache указатель на \link HyScanCache \endlink;
 *
 * \return указатель на \link HyScanTileColor \endlink.
 */
HYSCAN_API
HyScanTileColor        *hyscan_tile_color_new                  (HyScanCache            *cache);
/**
 *
 * Функция "открывает" БД, проект и галс.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param db_uri путь к БД;
 * \param project имя проекта;
 * \param track имя галса.
 *
 * \return указатель на \link HyScanTileColor \endlink.
 */
HYSCAN_API
void                    hyscan_tile_color_open                 (HyScanTileColor        *color,
                                                                const gchar            *db_uri,
                                                                const gchar            *project,
                                                                const gchar            *track);

/**
 *
 * Функция "закрывает" БД, проект и галс.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 *
 */
HYSCAN_API
void                    hyscan_tile_color_close                (HyScanTileColor        *color);

/**
 *
 * Функция ищет тайл в кэше.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param requested_tile запрошенный тайл;
 * \param cached_tile тайл, хранящийся в кэше.
 *
 * \return TRUE, если тайл найден.
 */
HYSCAN_API
gboolean                hyscan_tile_color_check                (HyScanTileColor        *color,
                                                                HyScanTile             *requested_tile,
                                                                HyScanTile             *cached_tile);

/**
 *
 * Функция получает тайл из кэша и кладет его в соответствующий cairo_surface_t.
 * При этом если размеры surface не совпадают с размерами тайла, он автоматически
 * пересоздается.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param requested_tile запрошенный тайл;
 * \param cached_tile кэшированный тайл;
 * \param surface структура \link HyScanTileSurface \endlink с информацией об
 * области, в которую следует отрисовать тайл.
 *
 * \return TRUE, если тайл найден.
 */
HYSCAN_API
gboolean                hyscan_tile_color_get                  (HyScanTileColor        *color,
                                                                HyScanTile             *requested_tile,
                                                                HyScanTile             *cached_tile,
                                                                HyScanTileSurface      *surface);

/**
 *
 * Функция раскрашивает тайл, кэширует его и кладет в cairo_surface_t.
 * При этом если размеры surface не совпадают с размерами тайла, он автоматически
 * пересоздается.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param tile новый тайл;
 * \param input кадр (в формате с плавающей точкой);
 * \param size размер тайла;
 * \param surface структура \link HyScanTileSurface \endlink с информацией об
 * области, в которую следует отрисовать тайл.
 *
 */
HYSCAN_API
void                    hyscan_tile_color_add                  (HyScanTileColor        *color,
                                                                HyScanTile             *tile,
                                                                gfloat                 *input,
                                                                gint                    size,
                                                                HyScanTileSurface      *surface);

/**
 *
 * Функция устанавливает уровни черной и белой точки и значение гаммы.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param source тип источника, к которому будут применены параметры;
 * \param black уровень черной точки в диапазоне [0; white);
 * \param gamma гамма (больше нуля);
 * \param white уровень белой точки в диапазоне [0; 1].
 *
 * \return TRUE, если уровни успешно установлены.
 */
HYSCAN_API
gboolean                hyscan_tile_color_set_levels           (HyScanTileColor        *color,
                                                                HyScanSourceType        source,
                                                                gdouble                 black,
                                                                gdouble                 gamma,
                                                                gdouble                 white);
/**
 *
 * Функция устанавливает уровни черной и белой точки и значение гаммы
 * для всех каналов, для которых не заданы эти уровни.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param black уровень черной точки в диапазоне [0; white);
 * \param gamma гамма (больше нуля);
 * \param white уровень белой точки в диапазоне [0; 1].
 *
 * \return TRUE, если уровни успешно установлены.
 */
HYSCAN_API
gboolean                hyscan_tile_color_set_levels_for_all   (HyScanTileColor        *color,
                                                                gdouble                 black,
                                                                gdouble                 gamma,
                                                                gdouble                 white);

/**
 *
 * Функция устанавливает цветовую схему.
 * Можно передать массив любого размера (больше нуля).
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param source тип источника, к которому будут применены параметры;
 * \param colormap значения цветов точек;
 * \param length число элементов в цветовой схеме;
 * \param background цвет фона.
 *
 * \return TRUE, если цвета успешно установлены.
 */
HYSCAN_API
gboolean                hyscan_tile_color_set_colormap         (HyScanTileColor        *color,
                                                                HyScanSourceType        source,
                                                                guint32                *colormap,
                                                                guint                   length,
                                                                guint32                 background);
/**
 *
 * Функция устанавливает цветовую схему для всех каналов,
 * для которых не задана цветовая схема.
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param colormap значения цветов точек;
 * \param length число элементов в цветовой схеме;
 * \param background цвет фона.
 *
 * \return TRUE, если цвета успешно установлены.
 */
HYSCAN_API
gboolean                hyscan_tile_color_set_colormap_for_all (HyScanTileColor        *color,
                                                                guint32                *colormap,
                                                                guint                   length,
                                                                guint32                 background);

/**
 *
 * Функция рассчета цветовой палитры. Пока что поддерживается только 256 градаций.
 *
 * На вход подается массив цветов, которые будут равномерно распределены по цветовой схеме.
 * \param colors указатель на массив цветов;
 * \param num число элементов.
 * \param length число элементов в возвращенном массиве.
 *
 * \return Указатель на массив из 256 элементов с цветовой схемой.
 * После использования этот массив необходимо освободить.
 *
 */
HYSCAN_API
guint32                *hyscan_tile_color_compose_colormap     (guint32                *colors,
                                                                guint                   num,
                                                                guint                  *length);

/**
 *
 * Функция переводит значение цвета из отдельных компонентов в упакованное 32-х битное значение
 *
 * \param red красная составляющая;
 * \param green синяя составляющая;
 * \param blue зеленая составляющая;
 * \param alpha прозрачность.
 *
 * \return упакованное значение цвета.
 */
HYSCAN_API
guint32                 hyscan_tile_color_converter_d2i        (gdouble                 red,
                                                                gdouble                 green,
                                                                gdouble                 blue,
                                                                gdouble                 alpha);

/**
 *
 * Функция переводит значение цвета из 32-х битного значения в 4 значения с плавающей точкой.
 *
 * \param color упакованное значение цвета;
 * \param red красная составляющая;
 * \param green синяя составляющая;
 * \param blue зеленая составляющая;
 * \param alpha прозрачность.
 *
 */
HYSCAN_API
void                    hyscan_tile_color_converter_i2d        (guint32                 color,
                                                                gdouble                *red,
                                                                gdouble                *green,
                                                                gdouble                *blue,
                                                                gdouble                *alpha);

/**
 *
 * Функция переводит значение цвета из отдельных компонентов в упакованное 32-х битное значение
 *
 * \param red красная составляющая;
 * \param green синяя составляющая;
 * \param blue зеленая составляющая;
 * \param alpha прозрачность.
 *
 * \return упакованное значение цвета.
 *
 */
HYSCAN_API
guint32                 hyscan_tile_color_converter_c2i        (guchar                  red,
                                                                guchar                  green,
                                                                guchar                  blue,
                                                                guchar                  alpha);

G_END_DECLS

#endif /* __HYSCAN_TILE_COLOR_H__ */

/**
 *
 * \file hyscan-geo.h
 *
 * \brief Заголовочный файл функций и классов пересчета систем координат (СК),
 * используемых в проекте Hyscan. Определяет используемые при этом типы данных.
 *
 * \author Штанов Евгений (shtanov_evgenii@mail.ru)
 * \author Дмитриев Александр (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGeo HyScanGeo - класс для работы с системами координат.
 *
 * HyScanGeo - класс для работы с пересчетом геодезических координат в топоцентрические и обратно
 * и семейство функций для пересчета координат из различных СК.
 *
 * - - -
 * Терминология.
 *
 * 1. Датум - набор параметров, используемых для смещения и трансформации референц-эллипсоида в локальные
 *    географические координаты. Используются следующие датумы:
 *          - WGS84 (World Geodetic System 1984). Глобальный датум, использующий геоцентрический общемировой эллипсоид,
 *            вычисленный по результатам точных спутниковых измерений. Используется в системе GPS.
 *            В настоящее время принят как основной в США.
 *          - Пулково-1942 (СК-42, Система координат 1942) Локальный датум, использующий эллипсоид Красовского,
 *            максимально подходящего к европейской территории СССР. Основной (по распространенности) датум
 *            в СССР и постсоветском пространстве.
 *          - ПЗ-90 (Параметры Земли 1990) Глобальный датум, основной (с 2012 года) в Российской Федерации.
 *
 * 2. Геодезическая система координаты - сферическая СК, в которой координаты задаются широтой, долготой и высотой.
 * 3. Геоцентрическая система координат - декартова СК с началом в центре эллипсоида.
 * 4. Топоцентрическая система координат - прямоугольная СК, точка отсчета расположена на поверхности референц-
 *     эллипсоида, от нее отсчитывается геодезическая высота по нормали к эллипсоиду в плоскости первого вертикала (ось Z).
 *     Направление оси Y задается азимутом A0. Ось X дополняет систему до правой.
 *
 * Долгота (lon, longitude) отсчитывается от 0 меридиана на 180 градусов в обе стороны: [-PI, PI].
 * Широта (lat, latitude) отсчитывается от экватора на 90 градусов в обе стороны: [-PI/2, PI/2].
 *
 * Все угловые величины измеряются в радианах, линейные - в метрах.
 * - - -
 * Хранение данных.
 *
 * Данные о координатах хранятся в следующих структурах:
 *  - #HyScanGeoCartesian3D - прямоугольные координаты вида (x,y,z) или (x,y,h).
 *  - #HyScanGeoCartesian2D - прямоугольные координаты вида (x,y).
 *  - #HyScanGeoGeodetic - геодезические координаты вида (B,L,H) или (B,L,A), где: <BR>
 *
 * - - -
 * Методы класса HyScanGeo.
 * - #hyscan_geo_new - создание нового объекта с заданной точкой и типом эллипсоида;
 * - #hyscan_geo_new_user - создание нового объекта с пользовательским эллипсоидом;
 * - #hyscan_geo_set_origin - смена начала координат топоцентрической СК;
 * - #hyscan_geo_set_origin_user - смена начала координат топоцентрической СК и с пользовательским эллипсоидом;
 * - #hyscan_geo_geo2topo - перевод геоцентрических координат в топоцентрические;
 * - #hyscan_geo_topo2geo - перевод топоцентрических координат в геоцентрические;
 * - #hyscan_geo_geo2topoXY - перевод геоцентрических координат в топоцентрические без учета высоты;
 * - #hyscan_geo_topoXY2geo - перевод топоцентрических координат в геоцентрические без учета высоты.
 *
 * - - -
 * Функции пересчета координат.
 * - #hyscan_geo_cs_transform - перевод координат из одной геодезической СК в другую;
 * - #hyscan_geo_cs_transform_user - перевод координат из одной геодезической СК в другую с использованием пользователского датума;
 * - #hyscan_geo_get_datum_params - получение параметров преобразования Гельмерта между системами координат;
 * - #hyscan_geo_get_helmert_params_to_wgs84 - получение параметров преобразования Гельмерта для пересчета в СК WGS-84;
 * - #hyscan_geo_init_ellipsoid - вычисление параметров референц-эллипсоида по его типу;
 * - #hyscan_geo_init_ellipsoid_user - вычисление параметров референц-эллипсоида по двум параметрам (большая полуось и сжатие);
 * - #hyscan_geo_get_ellipse_params - получение основных параметров эллипсоида по его типу;
 * - - -
 * Работа класса.
 *
 * Объект создается с помощью hyscan_geo_new. При этом передается тип эллипсоида и координаты начальной точки.
 * При этом инициализируются структуры в private-секции. Сразу после этого объект готов к работе.
 *
 */

#ifndef __HYSCAN_GEO_H__
#define __HYSCAN_GEO_H__

#include <glib-object.h>
#include <hyscan-types.h>

G_BEGIN_DECLS

/** \brief Используемые системы координат. */
typedef enum
{
  HYSCAN_GEO_CS_INVALID, /**< Некорректная СК. */
  HYSCAN_GEO_CS_WGS84,   /**< WGS84. */
  HYSCAN_GEO_CS_SK42,    /**< SK42. */
  HYSCAN_GEO_CS_SK95,    /**< SK95. */
  HYSCAN_GEO_CS_PZ90,    /**< PZ90. */
  HYSCAN_GEO_CS_PZ90_02, /**< PZ90_02. */
  HYSCAN_GEO_CS_PZ90_11  /**< PZ90_11. */
} HyScanGeoCSType;

/** \brief Типы используемых референц-эллипсоидов */
typedef enum
{
  HYSCAN_GEO_ELLIPSOID_INVALID,    /**< Некорректный эллипсоид. */
  HYSCAN_GEO_ELLIPSOID_WGS84,      /**< Эллипсоид WGS-84. */
  HYSCAN_GEO_ELLIPSOID_KRASSOVSKY, /**< Эллипсоид Крассовского. */
  HYSCAN_GEO_ELLIPSOID_PZ90        /**< Эллипсоид ПЗ-90. */
} HyScanGeoEllipsoidType;

/** \brief Геодезические координаты (B, L, H) или (lat, long, H). */
typedef struct
{
  gdouble lat;             /**< Широта. */
  gdouble lon;             /**< Долгота. */
  gdouble h;               /**< Высота или азимут (направление оси Y топоцентрической СК,
                                отмеряется относительно северного направления долготы по часовой стрелке). */
} HyScanGeoGeodetic;

/** \brief Декартовы координаты в пространстве ((X, Y, Z) или (X, Y, H)). */
typedef struct
{
  gdouble x;               /**< x. */
  gdouble y;               /**< y. */
  gdouble z;               /**< z. */
} HyScanGeoCartesian3D;

/** \brief Декартовы координаты на плоскости ((X, Y)). */
typedef struct
{
  gdouble x;               /**< x. */
  gdouble y;               /**< y. */
} HyScanGeoCartesian2D;

/** \brief Параметры пересчета кординат, датум. */
typedef struct
{
  gdouble dX;              /**< Линейный сдвиг */
  gdouble dY;              /**< Линейный сдвиг */
  gdouble dZ;              /**< Линейный сдвиг */
  gdouble wx;              /**< Коэффициент матрицы поворота. */
  gdouble wy;              /**< Коэффициент матрицы поворота. */
  gdouble wz;              /**< Коэффициент матрицы поворота. */
  gdouble m;               /**< Коэффициент масштаба. */
} HyScanGeoDatumParam;

/** \brief Структура, хранящая необходимые для пересчета координат параметры эллипсоида. */
typedef struct
{
  gdouble  a;              /**< Большая полуось. */
  gdouble  b;              /**< Малая полуось. */
  gdouble  f;              /**< Полярное сжатие. */
  gdouble  c;              /**< Полярный радиус кривизны поверхности. */
  gdouble  e;              /**< Первый эксцентриситет. */
  gdouble  e2;             /**< Квадрат первого эксцентриситета. */
  gdouble  e12;            /**< Квадрат второго эксцентриситета. */
} HyScanGeoEllipsoidParam;

#define HYSCAN_TYPE_GEO             (hyscan_geo_get_type ())
#define HYSCAN_GEO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GEO, HyScanGeo))
#define HYSCAN_IS_GEO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GEO))
#define HYSCAN_GEO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GEO, HyScanGeoClass))
#define HYSCAN_IS_GEO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GEO))
#define HYSCAN_GEO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GEO, HyScanGeoClass))

typedef struct HyScanGeo HyScanGeo;
typedef struct HyScanGeoPrivate HyScanGeoPrivate;
typedef struct HyScanGeoClass HyScanGeoClass;

struct HyScanGeo
{
  GObject            parent_instance;
  HyScanGeoPrivate  *priv;
};

struct HyScanGeoClass
{
  GObjectClass       parent_class;
};

HYSCAN_API
GType                   hyscan_geo_get_type                    (void);

/**
 *
 * Функция создаёт объект HyScanGeo, устанавливает начало топоцентрических координат.
 *
 * \param[in] origin - точка отсчета топоцентрических координат: геодезические координаты (lat, lon) и азимут;
 * \param[in] ell_type - тип референц - эллипсоида.
 * \return NULL при некорректных входных данных
 */
HYSCAN_API
HyScanGeo              *hyscan_geo_new                         (HyScanGeoGeodetic               origin,
                                                                HyScanGeoEllipsoidType          ell_type);

/**
 *
 * Функция (конструктор) создаёт объект HyScanGeo, устанавливает начало топоцентрических координат.
 *
 * \param[in] origin - точка отсчета топоцентрических координат: геодезические координаты (lat, lon) и азимут;
 * \param[in] ell_params - параметры референц - эллипсоида, структура #HyScanGeoEllipsoidParam;
 *  должна быть инициализирована с помощью функции  #hyscan_geo_init_ellipsoid_user или #hyscan_geo_init_ellipsoid.
 * \return указатель на NULL при некорректных входных данных
 */
HYSCAN_API
HyScanGeo              *hyscan_geo_new_user                    (HyScanGeoGeodetic               origin,
                                                                HyScanGeoEllipsoidParam         ell_params);

/**
 *
 * Эта функция должна быть вызвана при смене начала координат.
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[in] origin - точка отсчета топоцентрических координат: геодезические координаты и азимут;
 * \param[in] ell_type - тип референц - эллипсоида.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_set_origin                  (HyScanGeo                      *geo,
                                                                HyScanGeoGeodetic               origin,
                                                                HyScanGeoEllipsoidType          ell_type);

/**
 *
 * Эта функция должна быть вызвана при смене начала координат.
 * Отличие от #hyscan_geo_set_origin  в том,
 * что параметры эллипсоида нужно передать как структуру #HyScanGeoEllipsoidParam.
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[in] origin - точка отсчета топоцентрических координат: геодезические координаты и азимут;
 * \param[in] ell_params - параметры референц - эллипсоида, структура #HyScanGeoEllipsoidParam;
 * должна быть инициализирована с помощью функции #hyscan_geo_init_ellipsoid_user или #hyscan_geo_init_ellipsoid.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_set_origin_user             (HyScanGeo                      *geo,
                                                                HyScanGeoGeodetic               origin,
                                                                HyScanGeoEllipsoidParam         ell_params);
/**
 *
 * Эта функция задает количество итераций, смотри #hyscan_geo_topoXY2geo.
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[in] iters - количество итераций.
 *
 */
HYSCAN_API
void                    hyscan_geo_set_number_of_iterations    (HyScanGeo                      *geo,
                                                                guint                           iters);

/**
 *
 * Функция возвращает значение флага initialized.
 * Иными словами, она показывает, готов ли объект к работе (то есть установлена начальная точка и заданы параметры эллипсоида).
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[in] uninit - TRUE, если нужно сбросить флаг инициализации.
 *
 * \return TRUE, если объект инициализирован.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_ready                       (HyScanGeo                      *geo,
                                                                gboolean                        uninit);

/**
 *
 * Пересчет геодезических координат в топоцентрическую СК.
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[out] dst_topo - топоцентрические координаты на выходе, указатель на структуру #HyScanGeoCartesian3D;
 * \param[in] src_geod - геодезические координаты на входе, структура #HyScanGeoGeodetic.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_geo2topo                    (HyScanGeo                      *geo,
                                                                HyScanGeoCartesian3D           *dst_topo,
                                                                HyScanGeoGeodetic               src_geod);

/**
 *
 * Пересчет топоцентрических координат в геодезические.
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[out] dst_geod - геодезические координаты на выходе, указатель на структуру #HyScanGeoGeodetic;
 * \param[in] src_topo - топоцентрические координаты на входе, структура #HyScanGeoCartesian3D.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_topo2geo                    (HyScanGeo                      *geo,
                                                                HyScanGeoGeodetic              *dst_geod,
                                                                HyScanGeoCartesian3D            src_topo);

/**
 *
 * Пересчет геодезических координат в топоцентрическую СК в плоскость XOY.
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[out] dst_topoXY - топоцентрические координаты X, Y на выходе, указатель на структуру #HyScanGeoCartesian2D;
 * \param[in] src_geod - геодезические координаты на входе, структура #HyScanGeoGeodetic.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_geo2topoXY                  (HyScanGeo                      *geo,
                                                                HyScanGeoCartesian2D           *dst_topoXY,
                                                                HyScanGeoGeodetic               src_geod);

/**
 *
 * \brief Пересчет топоцентрических координат X, Y в геодезические B, L, H (широта, долгота, геодезическая высота).
 * При этом применяется итерационный процесс, число итераций задается параметром num_of_iter. Если он равен 0, то для
 * аппроксимации поверхности Земли используется сфера, иначе сфероид. Чем больше число итераций, тем выше точность.
 * |       Ошибка пересчета в метрах         |            |            |            |            |
 * |:---------------------------------------:|:----------:|:----------:|:----------:|:----------:|
 * | Удаление от центра координат в плане, м | 0 итераций | 1 итерация | 2 итерации | 3 итерации |
 * |                  10000                  |   0.0001   |            |            |            |
 * |                  100000                 |     0.1    |   0.00005  |            |            |
 * |                  100000                 |     200    |     10     |     0.5    |    0.02    |
 *
 * \param[in] geo - указатель на объект класcа;
 * \param[out] dst_geod - геодезические координаты на выходе, указатель на структуру. При этом
      высота копируется из h_geodetic;
 * \param[in] src_topoXY - топоцентрические координаты X, Y на входе, структура #HyScanGeoCartesian2D.
 * \param[in] h_geodetic - геодезическая высота конечной точки, gdouble;
 * \param[in] num_of_iter - 0, если для аппроксимации используется сфера. Чем больше значение, тем выше точность,
 * но дольше идут вычисления.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_topoXY2geo                  (HyScanGeo                      *geo,
                                                                HyScanGeoGeodetic              *dst_geod,
                                                                HyScanGeoCartesian2D            src_topoXY,
                                                                gdouble                         h_geodetic);

/* Функции для пересчета геодезических координат в различные СК. */

/**
 *
 * Пересчет геодезических координат точки src с датумом datum_in в точку dst с датумом datum_out.
 *
 * \param[out] dst - выходная координата, указатель на структуру #HyScanGeoGeodetic;
 * \param[in] src - входная координата, структура #HyScanGeoGeodetic;
 * \param[in] cs_in - тип входной СК, структура #HyScanGeoCSType;
 * \param[in] cs_out - тип выходной СК, структура #HyScanGeoCSType.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_cs_transform                  (HyScanGeoGeodetic            *dst,
                                                                  HyScanGeoGeodetic             src,
                                                                  HyScanGeoCSType               cs_in,
                                                                  HyScanGeoCSType               cs_out);

/**
 *
 * Пересчет геодезических координат точки src в точку dst из СК с референц-эллипсоидом el_params_in в
 * СК с референц-эллипсоидом el_params_out с использованием датума datum_param.
 * Используется при известных параметрах преобрзования (датума, параметров референц-эллипсоида).
 *
 * \param[out] dst - выходная координата, указатель на структуру #HyScanGeoGeodetic;
 * \param[in] src - входная координата, структура #HyScanGeoGeodetic;
 * \param[in] el_params_in - параметры референц-эллипсоида входной СК, структура #HyScanGeoEllipsoidParam;
 * \param[in] el_params_out - параметры референц-эллипсоида выходной СК, структура #HyScanGeoEllipsoidParam;
 * \param[in] datum_param - параметры пересчета, структура #HyScanGeoDatumParam.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_cs_transform_user             (HyScanGeoGeodetic            *dst,
                                                                  HyScanGeoGeodetic             src,
                                                                  HyScanGeoEllipsoidParam       el_params_in,
                                                                  HyScanGeoEllipsoidParam       el_params_out,
                                                                  HyScanGeoDatumParam           datum_param);

/**
 * Функция для получения параметров преобразования Гельмерта между
 * системами координат sc_in и sc_out
 *
 * \param cs_in - тип входной СК, структура #HyScanGeoCSType
 * \param cs_out - тип выходной СК, структура #HyScanGeoCSType
 */
HYSCAN_API
HyScanGeoDatumParam     hyscan_geo_get_datum_params            (HyScanGeoCSType                 cs_in,
                                                                HyScanGeoCSType                 cs_out);

/**
 * Функция для получения параметров преобразования Гельмерта для пересчета в СК WGS-84
 * по типу СК (локальной системы координат) #HyScanGeoCSType
 *
 * \param cs_type - тип системы координат #HyScanGeoCSType
 * \return структура с 7 параметрами для пересчета координат в СК WGS-84,
 * возвращает все параметры нулевыми, если передается datum - HYSCAN_GEO_DATUM_WGS84 или HYSCAN_GEO_CS_INVALID,
 * что соответствует единичному преобразованию (домножение на единичную матрицу и нулевой сдвиг)
 */
HYSCAN_API
HyScanGeoDatumParam     hyscan_geo_get_helmert_params_to_wgs84 (HyScanGeoCSType                 cs_type);


/** Функция вычисления параметров референц-эллипсоида по его типу #HyScanGeoEllipsoidType. */
HYSCAN_API
gboolean                hyscan_geo_init_ellipsoid              (HyScanGeoEllipsoidParam        *p,
                                                                HyScanGeoEllipsoidType          ell_type);

/**
 *
 * Функция вычисления параметров референц-эллипсоида по двум параметрам:
 *
 * \param[out] p - указатель на структуру с параметрами эллипсоида;
 * \param[in] a - большая полуось (major semiaxis);
 * \param[in] f - полярное сжатие (уплощение, flattening).
 *
 */
HYSCAN_API
gboolean                hyscan_geo_init_ellipsoid_user         (HyScanGeoEllipsoidParam        *p,
                                                                gdouble                         a,
                                                                gdouble                         f);

/**
 *
 * Функция получения основных параметров эллипсоида по его типу #HyScanGeoEllipsoidType.
 *
 * \param[out] a - большая полуось (major semiaxis);
 * \param[out] f - полярное сжатие (уплощение, flattening);
 * \param[out] epsg - EPSG код референц-эллипсоида;
 * \param[in] ell_type - тип референц-эллипсоида #HyScanGeoEllipsoidType;
 * \return TRUE, если эллипсоид найден, иначе FALSE.
 *
 */
HYSCAN_API
gboolean                hyscan_geo_get_ellipse_params          (gdouble                        *a,
                                                                gdouble                        *f,
                                                                gdouble                        *epsg,
                                                                HyScanGeoEllipsoidType          ell_type);


G_END_DECLS
#endif /* __HYSCAN_GEO_H__ */

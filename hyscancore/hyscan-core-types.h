/**
 * \file hyscan-core-types.h
 *
 * \brief Заголовочный файл типов объектов используемых в HyScanCore
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCoreTypes HyScanCoreTypes - типы объектов используемых в HyScanCore
 *
 * В HyScanCoreTypes вводятся определения следующих структур:
 *
 * - \link HyScanAntennaPosition \endlink - параметры местоположения приёмной антенны;
 * - \link HyScanRawDataInfo \endlink - параметры "сырых" гидролокационных данных.
 * - \link HyScanAcousticDataInfo \endlink - параметры акустических данных.
 *
 * В структуре \link HyScanAntennaPosition \endlink присутствует информация о местоположении
 * приёмных антенн. Смещения приёмной антенны указываются относительно центра масс судна.
 * При этом ось X направлена в нос, ось Y на правый борт, ось Z вверх.
 *
 * Углы установки антенны указываются для вектора, перпиндикулярного её рабочей
 * плоскости. Угол psi учитывает разворот антенны по курсу от её нормального положения.
 * Угол gamma учитывает разворот антенны по крену. Угол theta учитывает разворот
 * антенны по дифференту от её нормального положения.
 *
 * Положительные значения указываются для углов отсчитываемых против часовой стрелки.
 *
 */

#ifndef __HYSCAN_CORE_TYPES_H__
#define __HYSCAN_CORE_TYPES_H__

#include <hyscan-types.h>

G_BEGIN_DECLS

/** \brief Параметры местоположения приёмной антенны */
typedef struct
{
  gdouble                      x;                              /**< Смещение антенны по оси X, метры. */
  gdouble                      y;                              /**< Смещение антенны по оси Y, метры. */
  gdouble                      z;                              /**< Смещение антенны по оси Z, метры. */

  gdouble                      psi;                            /**< Поворот антенны по курсу, радианы. */
  gdouble                      gamma;                          /**< Поворот антенны по крену, радианы. */
  gdouble                      theta;                          /**< Поворот антенны по дифференту, радианы. */
} HyScanAntennaPosition;

/** \brief Параметры сырых гидролокационных данных */
typedef struct
{
  struct                                                       /**< Параметры данных. */
  {
    HyScanDataType             type;                           /**< Тип данных. */
    gdouble                    rate;                           /**< Частота дискретизации, Гц. */
  } data;

  struct                                                       /**< Параметры антенны. */
  {
    struct
    {
      gdouble                  vertical;                       /**< Смещение антенны в "решётке" в вертикальной плоскости, м. */
      gdouble                  horizontal;                     /**< Смещение антенны в "решётке" в горизонтальной плоскости, м. */
    } offset;

    struct
    {
      gdouble                  vertical;                       /**< Диаграмма направленности в вертикальной плоскости, рад. */
      gdouble                  horizontal;                     /**< Диаграмма направленности в горизонтальной плоскости, рад. */
    } pattern;

    gdouble                    frequency;                      /**< Центральная частота, Гц. */
    gdouble                    bandwidth;                      /**< Полоса пропускания, Гц. */
  } antenna;

  struct                                                       /**< Параметры АЦП. */
  {
    gdouble                    vref;                           /**< Опорное напряжение, В. */
    gint                       offset;                         /**< Смещение нуля, отсчёты. */
  } adc;
} HyScanRawDataInfo;

/** \brief Параметры акустических данных */
typedef struct
{
  struct                                                       /**< Параметры данных. */
  {
    HyScanDataType             type;                           /**< Тип данных. */
    gdouble                    rate;                           /**< Частота дискретизации, Гц. */
  } data;

  struct                                                       /**< Параметры антенны. */
  {
    struct
    {
      gdouble                  vertical;                       /**< Диаграмма направленности в вертикальной плоскости, рад. */
      gdouble                  horizontal;                     /**< Диаграмма направленности в горизонтальной плоскости, рад. */
    } pattern;
  } antenna;
} HyScanAcousticDataInfo;

G_END_DECLS

#endif /* __HYSCAN_CORE_TYPES_H__ */

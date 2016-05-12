/*
 * \file hyscan-data-channel-common.h
 *
 * \brief Заголовочный файл общих типов данных HyScanDataChannel
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_DATA_CHANNEL_COMMON_H__
#define __HYSCAN_DATA_CHANNEL_COMMON_H__

#include <hyscan-data.h>

/**
 *
 * \brief Параметры канала с акустическими данными
 *
 * Смещения приёмной антенны канала данных указываются относительно центра масс
 * судна. При этом ось X направлена в нос, ось Y на правый борт, ось Z вверх.
 *
 * Углы установки антенны указываются для вектора, перпиндикулярного её рабочей
 * плоскости. Угол psi учитывает разворот антенны по курсу от её нормального положения.
 * Угол gamma учитывает разворот антенны по крену. Угол theta учитывает разворот
 * антенны по дифференту от её нормального положения.
 *
 * Положительные значения указываются для углов отсчитываемых против часовой стрелки.
 *
 */
typedef struct
{
  HyScanDataType               discretization_type;            /**< Тип дискретизации данных. */
  gdouble                      discretization_frequency;       /**< Частота дискретизации данных. */

  gdouble                      vertical_pattern;               /**< Диаграмма направленности антенны в вертикальной плоскости. */
  gdouble                      horizontal_pattern;             /**< Диаграмма направленности антенны в горизонтальной плоскости. */

  gdouble                      x;                              /**< Смещение антенны по оси X, метры. */
  gdouble                      y;                              /**< Смещение антенны по оси Y, метры. */
  gdouble                      z;                              /**< Смещение антенны по оси Z, метры. */

  gdouble                      psi;                            /**< Поворот антенны по курсу, радианы. */
  gdouble                      gamma;                          /**< Поворот антенны по крену, радианы. */
  gdouble                      theta;                          /**< Поворот антенны по дифференту, радианы. */
} HyScanDataChannelInfo;

#endif /* __HYSCAN_DATA_CHANNEL_COMMON_H__ */

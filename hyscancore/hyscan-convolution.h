/**
 * \file hyscan-convolution.h
 *
 * \brief Заголовочный файл класса свёртки данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanConvolution HyScanConvolution - класс свёртки данных
 *
 * Класс HyScanConvolution используется для выполнения свёртки данных с образцом. Данные и образец
 * для свёртки должны быть представлены в комплексном виде (\link HyScanComplexFloat \endlink).
 *
 * Объект для выполнения свёртки создаётся функцией #hyscan_convolution_new.
 *
 * Перед выполнением свёртки необходимо задать образец с которым будет выполняться свёртка. Для
 * этого предназначена функция #hyscan_convolution_set_image. Образец для свёртки можно изменять
 * в процессе работы с объектом. При повторных вызовах функции #hyscan_convolution_set_image
 * будет установлен новый образец для свёртки.
 *
 * Функция #hyscan_convolution_convolve выполняет свертку данных.
 *
 * HyScanConvolution не поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_CONVOLUTION_H__
#define __HYSCAN_CONVOLUTION_H__

#include <glib-object.h>
#include <hyscan-data.h>
#include <hyscan-core-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CONVOLUTION             (hyscan_convolution_get_type ())
#define HYSCAN_CONVOLUTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CONVOLUTION, HyScanConvolution))
#define HYSCAN_IS_CONVOLUTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CONVOLUTION))
#define HYSCAN_CONVOLUTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CONVOLUTION, HyScanConvolutionClass))
#define HYSCAN_IS_CONVOLUTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CONVOLUTION))
#define HYSCAN_CONVOLUTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CONVOLUTION, HyScanConvolutionClass))

typedef struct _HyScanConvolution HyScanConvolution;
typedef struct _HyScanConvolutionPrivate HyScanConvolutionPrivate;
typedef struct _HyScanConvolutionClass HyScanConvolutionClass;

struct _HyScanConvolution
{
  GObject parent_instance;

  HyScanConvolutionPrivate *priv;
};

struct _HyScanConvolutionClass
{
  GObjectClass parent_class;
};

HYSCAN_CORE_EXPORT
GType hyscan_convolution_get_type (void);

/**
 *
 * Функция создаёт новый объект \link HyScanConvolution \endlink.
 *
 * \return Указатель на объект \link HyScanConvolution \endlink.
 *
 *
 */
HYSCAN_CORE_EXPORT
HyScanConvolution     *hyscan_convolution_new          (void);

/**
 *
 * Функция задаёт образец сигнала для свёртки.
 *
 * \param convolution указатель на объект \link HyScanConvolution \endlink;
 * \param image образец для свёртки;
 * \param n_points размер образца в точках.
 *
 * \return TRUE - если образец для свёртки установлен, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_convolution_set_image    (HyScanConvolution     *convolution,
                                                        HyScanComplexFloat    *image,
                                                        gint32                 n_points);

/**
 *
 * Функция выполняет свёртку данных с образцом. Результат свёртки помещается
 * во входной массив.
 *
 * \param convolution указатель на объект \link HyScanConvolution \endlink;
 * \param data данные для свёртки;
 * \param n_points размер данных в точках.
 *
 * \return TRUE - если свёртка выполнена, FALSE - в случае ошибки.
 *
 */
HYSCAN_CORE_EXPORT
gboolean               hyscan_convolution_convolve     (HyScanConvolution     *convolution,
                                                        HyScanComplexFloat    *data,
                                                        gint32                 n_points);

G_END_DECLS

#endif /* __HYSCAN_CONVOLUTION_H__ */

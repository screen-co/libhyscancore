/**
 * \file hyscan-depthometer.h
 *
 * \brief Заголовочный файл класса определения и аппроксимации глубины по времени.
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanDepthometer HyScanDepthometer - определение глубины в произвольный
 * момент времени.
 *
 * HyScanDepthometer находится на более высоком уровне, чем \link HyScanDepth \endlink.
 * Если HyScanDepth работает непосредственно с каналами данных, то HyScanDepthometer ничего
 * не знает о конкретных источниках данных. Однако он занимается определением глубины не для
 * индекса, а для произвольного времени.
 *
 * Класс не является потокобезопасным. Для работы из разных потоков рекомендуется
 * создавать свою пару объектов HyScanDepthometer и HyScanDepth.
 *
 * Доступны следующие методы:
 * - #hyscan_depthometer_new - создает новый объект определения глубины;
 * - #hyscan_depthometer_set_cache - устанавливает кэш;
 * - #hyscan_depthometer_set_filter_size - устанавливает размер окна для аппроксимации.
 * - #hyscan_depthometer_set_validity_time - устанавливает время валидности данных;
 * - #hyscan_depthometer_get - возвращает глубину в момент времени.
 */
#ifndef __HYSCAN_DEPTHOMETER_H__
#define __HYSCAN_DEPTHOMETER_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEPTHOMETER             (hyscan_depthometer_get_type ())
#define HYSCAN_DEPTHOMETER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEPTHOMETER, HyScanDepthometer))
#define HYSCAN_IS_DEPTHOMETER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEPTHOMETER))
#define HYSCAN_DEPTHOMETER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DEPTHOMETER, HyScanDepthometerClass))
#define HYSCAN_IS_DEPTHOMETER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DEPTHOMETER))
#define HYSCAN_DEPTHOMETER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DEPTHOMETER, HyScanDepthometerClass))

typedef struct _HyScanDepthometer HyScanDepthometer;
typedef struct _HyScanDepthometerPrivate HyScanDepthometerPrivate;
typedef struct _HyScanDepthometerClass HyScanDepthometerClass;

struct _HyScanDepthometer
{
  GObject parent_instance;

  HyScanDepthometerPrivate *priv;
};

struct _HyScanDepthometerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_depthometer_get_type             (void);

/**
 * Функция создает новый объект получения глубины.
 *
 * \param depth интерфейс \link HyScanDepth \endlink.
 *
 * \return объект HyScanDepthometer.
 */
HYSCAN_API
HyScanDepthometer      *hyscan_depthometer_new                 (HyScanNavData            *depth);

/**
 *
 * Функция устанавливает кэш.
 *
 * \param depthometer объект \link HyScanDepthometer \endlink;
 * \param cache интерфейс \link HyScanCache \endlink;
 *
 */
HYSCAN_API
void                    hyscan_depthometer_set_cache           (HyScanDepthometer      *depthometer,
                                                                HyScanCache            *cache);

/**
 *
 * Функция устанавливает число точек для аппроксимации.
 *
 * Число точек должно быть больше нуля. Так как классу требуется четное число
 * точек, то при передаче нечетного значения оно будет автоматически инкрементировано.
 *
 * \param depthometer объект \link HyScanDepthometer \endlink;
 * \param size число точек, используемых для определения глубины.
 *
 * \return TRUE, если удалось установить новое значение.
 */
HYSCAN_API
gboolean                hyscan_depthometer_set_filter_size     (HyScanDepthometer      *depthometer,
                                                                guint                   size);

/**
 *
 * Функция устанавливает окно валидности данных.
 *
 * Данные могут быть запрошены не для того момента, для которого они реально
 * есть, а чуть раньше или позже. В этом случае есть два варианта:
 * либо интерполировать данные, либо "нарезать" временную шкалу так,
 * так что на каждом отрезке глубина считается постоянной. Функция задает
 * длину этого отрезка в микросекундах.
 *
 * \param depthometer объект \link HyScanDepthometer \endlink;
 * \param microseconds время в микросекундах, в течение которого считается, что
 * глубина не изменяется.
 *
 */
HYSCAN_API
void                    hyscan_depthometer_set_validity_time     (HyScanDepthometer    *depthometer,
                                                                  gint64                microseconds);

/**
 *
 * Функция возвращает глубину.
 * Если значение не найдено в кэше или кэш не установлен, функция
 * произведет все необходимые вычисления.
 *
 * \param depthometer объект \link HyScanDepthometer \endlink;
 * \param time время.
 *
 * \return глубина в запрошенный момент времени или -1.0 в случае ошибки.
 */
HYSCAN_API
gdouble                 hyscan_depthometer_get                 (HyScanDepthometer      *depthometer,
                                                                gint64                  time);
/**
 *
 * Функция возвращает глубину.
 * В отличие от #hyscan_depthometer_get эта функция только ищет
 * значение в кэше. Если кэш не задан или значение не присутствует в кэше,
 * будет возвращено значение -1.0.
 *
 * \param depthometer объект \link HyScanDepthometer \endlink;
 * \param time время.
 *
 * \return глубина в запрошенный момент времени или -1.0 в случае ошибки.
 */
HYSCAN_API
gdouble                 hyscan_depthometer_check               (HyScanDepthometer      *depthometer,
                                                                gint64                  time);

G_END_DECLS

#endif /* __HYSCAN_DEPTHOMETER_H__ */

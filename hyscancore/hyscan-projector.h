/**
 * \file hyscan-projector.h
 *
 * \brief Заголовочный файл класса сопоставления индексов и отсчётов реальным координатам
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanProjector HyScanProjector - класс сопоставления индексов и отсчётов реальным координатам
 *
 * Класс HyScanProjector позволяет определить координату индекса в канале данных и отсчёта
 * в акустической строке.
 *
 * Первая задача, решаемая классом - это определение координат определенной акустической
 * строки, то есть вдоль оси движения судна.
 * Поскольку в галсе могут присутствовать несколько каналов данных, для отрисовки наиболее
 * точной картины требуется учесть не только сдвиг приемников относительно друг друга,
 * но и сдвиг, вызванный различным началом времени приема. Для этого можно использовать
 * настоящий класс. Минимальная временная метка данных среди тех каналов, что интересуют
 * пользователя, считается абсолютным началом галса. Все координаты отсчитываются от этого
 * времени.
 * Определение координаты по индексу производится функцией #hyscan_projector_index_to_coord,
 * поиск индекса по координате - #hyscan_projector_find_index_by_coord
 *
 * Вторая задача - это определение координаты конкретного индекса в акустической строке.
 * На эту величину влияют частота дискретизации, скорость звука (или профиль скорости
 * звука), глубина, координата приемной антенны и, разумеется, номер отсчёта. Класс
 * позволяет учесть все эти величины.
 * Определение координаты по номеру отсчета производится функцией #hyscan_projector_count_to_coord,
 * определение отсчёта по координате производится функцией #hyscan_projector_coord_to_count.
 *
 * Для ускорения работы можно использовать функцию #hyscan_projector_set_precalc_points.
 * Она позволяет задать число предрассчитанных точек для определения координаты по номеру
 * отсчёта.
 *
 * Класс не является потокобезопасным.
 * Все координаты в метрах.
 *
 * - #hyscan_projector_new - создает новый объект;
 * - #hyscan_projector_check_source - добавляет источник данных;
 * - #hyscan_projector_set_ship_speed - задает скорость судна;
 * - #hyscan_projector_set_sound_velocity - задает профиль скорости звука;
 * - #hyscan_projector_set_precalc_points - задает число предрассчитанных точек;
 * - #hyscan_projector_find_index_by_coord - ищет индекс по координате;
 * - #hyscan_projector_index_to_coord - возвращает координату акустической строки (индекса);
 * - #hyscan_projector_count_to_coord - возвращает координату отсчёта;
 * - #hyscan_projector_coord_to_count - возвращает отсчёт по координате.
 */
#ifndef __HYSCAN_PROJECTOR_H__
#define __HYSCAN_PROJECTOR_H__

#include <hyscan-acoustic-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROJECTOR             (hyscan_projector_get_type ())
#define HYSCAN_PROJECTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROJECTOR, HyScanProjector))
#define HYSCAN_IS_PROJECTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROJECTOR))
#define HYSCAN_PROJECTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROJECTOR, HyScanProjectorClass))
#define HYSCAN_IS_PROJECTOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROJECTOR))
#define HYSCAN_PROJECTOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROJECTOR, HyScanProjectorClass))

typedef struct _HyScanProjector HyScanProjector;
typedef struct _HyScanProjectorPrivate HyScanProjectorPrivate;
typedef struct _HyScanProjectorClass HyScanProjectorClass;

struct _HyScanProjector
{
  GObject parent_instance;

  HyScanProjectorPrivate *priv;
};

struct _HyScanProjectorClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_projector_get_type               (void);

/**
 *
 * Функция создает новый объект.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project название проекта;
 * \param track название галса;
 * \param source тип источника данных;
 * \param raw использовать (TRUE) или нет (FALSE) сырые данные.
 *
 * \return указатель на объект HyScanProjector.
 */
HYSCAN_API
HyScanProjector        *hyscan_projector_new                    (HyScanDB          *db,
                                                                 HyScanCache       *cache,
                                                                 const gchar       *project,
                                                                 const gchar       *track,
                                                                 HyScanSourceType   source,
                                                                 guint              channel,
                                                                 gboolean           noise);

/**
 *
 * Функция проверяет источник данных.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param source тип источника данных;
 * \param raw использовать ли сырые данные;
 * \param changed флаг показывает, изменилось ли начальное время галса (или NULL).
 *
 * \return TRUE, если удалось определить самое раннее время в канале данных.
 */
HYSCAN_API
gboolean                hyscan_projector_check_source           (HyScanProjector   *projector,
                                                                 HyScanSourceType   source,
                                                                 gboolean           raw,
                                                                 gboolean          *changed);
/**
 *
 * Функция задает скорость судна.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param speed скорость судна в м/с.
 *
 * \return
 */
HYSCAN_API
void                    hyscan_projector_set_ship_speed         (HyScanProjector   *projector,
                                                                 gfloat             speed);
/**
 *
 * Функция задает профиль скорости звука.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param velocity профиль скорости звука (см HyScanSoundVelocity).
 */
HYSCAN_API
void                    hyscan_projector_set_sound_velocity     (HyScanProjector   *projector,
                                                                 GArray            *velocity);
/**
 *
 * Функция задает число предрассчитанных точек.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param points число точек.
 */
HYSCAN_API
void                    hyscan_projector_set_precalc_points     (HyScanProjector   *projector,
                                                                 guint32            points);

/**
 *
 * Функция возвращает объект работы с акустическими данными.
 * // TODO: унаследоваться от HyScanAcousticData?
 *
 * \param projector указатель на объект \link HyScanAcousticData \endlink;
 *
 * \return указатель на HyScanAcousticData.
 *
 */
HYSCAN_API
const HyScanAcousticData *hyscan_projector_get_acoustic_data    (HyScanProjector   *projector);
/**
 *
 * Функция ищет индекс по координате.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param coord координата;
 * \param lindex левый индекс данных (или NULL);
 * \param rindex правый индекс данных (или NULL).
 *
 * \return результат поиска.
 */
HYSCAN_API
HyScanDBFindStatus      hyscan_projector_find_index_by_coord    (HyScanProjector   *projector,
                                                                 gdouble            coord,
                                                                 guint32           *lindex,
                                                                 guint32           *rindex);
/**
 *
 * Функция определяет координату по индексу.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param index индекс;
 * \param along координата.
 *
 * \return TRUE, если удалось определить координату.
 */
HYSCAN_API
gboolean                hyscan_projector_index_to_coord         (HyScanProjector   *projector,
                                                                 guint32            index,
                                                                 gdouble           *along);

/**
 *
 * Функция определяет индекс строки по координате.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param along координата;
 * \param index индекс.
 *
 * \return
 */
HYSCAN_API
gboolean                hyscan_projector_coord_to_index         (HyScanProjector   *projector,
                                                                 gdouble            along,
                                                                 guint32           *index);

/**
 *
 * Функция определяет координату по номеру отсчёта.
 *
 * Для работы в наклонной дальности надо задать depth = 0.0.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param count номер отсчёта;
 * \param across координата;
 * \param depth глубина.
 *
 * \return TRUE, если удалось определить координату.
 */
HYSCAN_API
gboolean                hyscan_projector_count_to_coord         (HyScanProjector   *projector,
                                                                 guint32            count,
                                                                 gdouble           *across,
                                                                 gdouble            depth);

/**
 *
 * Функция определяет отсчёт по координате.
 *
 * \param projector указатель на объект \link HyScanProjector \endlink;
 * \param across координата;
 * \param count номер отсчёта;
 * \param depth глубина.
 *
 * \return TRUE, если удалось определить номер отсчёта.
 */
HYSCAN_API
gboolean                hyscan_projector_coord_to_count         (HyScanProjector   *projector,
                                                                 gdouble            across,
                                                                 guint32           *count,
                                                                 gdouble            depth);

G_END_DECLS

#endif /* __HYSCAN_PROJECTOR_H__ */

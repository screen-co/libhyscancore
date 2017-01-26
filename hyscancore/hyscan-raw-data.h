/**
 * \file hyscan-raw-data.h
 *
 * \brief Заголовочный файл класса обработки сырых данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanRawData HyScanRawData - класс обработки сырых данных
 *
 * Класс HyScanRawData используется для обработки сырых данных, такой как преобразование
 * типов данных, свёртки, вычисления аплитуды сигнала.
 *
 * Объект обработки данных создаётся функциями #hyscan_raw_data_new или #hyscan_raw_data_noise_new.
 *
 * В своей работе класс HyScanRawData может использовать внешний кэш для хранения обработанных
 * данных. В случае запроса ранее обработанных данных пользователь получит их копию из кэша. Несколько
 * экземпляров класса HyScanRawData, обрабатывающих одинаковые каналы данных могут использовать
 * один и тот же кэш. Таким образом, данные обработанные в одной части приложения не потребуют повторной
 * обработки в другой его части. Для задания кэша предназанчена функция #hyscan_raw_data_set_cache.
 *
 * Местоположение приёмной антенны гидролокатора можно получить функцией #hyscan_raw_data_get_position.
 * Параметры данных можно получить функцией #hyscan_raw_data_get_info.
 *
 * С помощью функций #hyscan_raw_data_get_source и #hyscan_raw_data_get_channel можно получить параметры
 * источника данных.
 *
 * Класс поддерживает обработку данных, запись которых производится в настоящий момент. В этом случае
 * могут появляться новые данные или исчезать уже записанные. Определить возможность изменения данных
 * можно с помощью функции #hyscan_raw_data_is_writable.
 *
 * Функции #hyscan_raw_data_get_range и #hyscan_raw_data_find_data используются для определения
 * границ записанных данных и их поиска по метке времени. Эти функции аналогичны функциям
 * \link hyscan_db_channel_get_data_range \endlink и \link hyscan_db_channel_find_data \endlink интерфейса
 * \link HyScanDB \endlink.
 *
 * Число точек данных, размер образа сигнала и время приёма данных можно получить с помощью функций
 * #hyscan_raw_data_get_values_count, #hyscan_raw_data_get_signal_size, #hyscan_raw_data_get_time.
 *
 * Для чтения данных используются следующие функции:
 *
 * - #hyscan_raw_data_get_signal_image - функция возвращает образ сигнала для свёртки;
 * - #hyscan_raw_data_get_tvg_values - функция возвращает значения коэффициентов усиления;
 * - #hyscan_raw_data_get_amplitude_values - функция возвращает амплитуду сигнала;
 * - #hyscan_raw_data_get_quadrature_values - функция возвращает квадратурные отсчёты сигнала.
 *
 * Функции чтения данных, используют в работе параметр buffer_size, определяющий размер буфера для
 * данных. Этот размер определяет число точек данных, способных поместиться в буфере.
 *
 * Функции  #hyscan_raw_data_get_amplitude_values и #hyscan_raw_data_get_quadrature_values могут
 * проводить обработку как с выполненим свёртки данных с образцом сигнала, так и без неё. Образцы сигналов для
 * свёртки хранятся вместе с данными. Имеется возможность временно отключить свёртку или включить её снова
 * функцией #hyscan_raw_data_set_convolve.
 *
 * После выполнения первичной обработки, данные возвращаются в формате gfloat или \link HyScanComplexFloat \endlink.
 * Амплитудные значения находятся в пределах от нуля до единицы, квадратурные от минус единицы до единицы.
 * Значения коэффициентов усиления приводятся в дБ.
 *
 * HyScanRawData не поддерживает работу в многопоточном режиме. Рекомендуется создавать свой экземпляр
 * объекта обработки данных в каждом потоке и использовать единый кэш данных.
 *
 */

#ifndef __HYSCAN_RAW_DATA_H__
#define __HYSCAN_RAW_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_RAW_DATA             (hyscan_raw_data_get_type ())
#define HYSCAN_RAW_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_RAW_DATA, HyScanRawData))
#define HYSCAN_IS_RAW_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_RAW_DATA))
#define HYSCAN_RAW_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_RAW_DATA, HyScanRawDataClass))
#define HYSCAN_IS_RAW_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_RAW_DATA))
#define HYSCAN_RAW_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_RAW_DATA, HyScanRawDataClass))

typedef struct _HyScanRawData HyScanRawData;
typedef struct _HyScanRawDataPrivate HyScanRawDataPrivate;
typedef struct _HyScanRawDataClass HyScanRawDataClass;

struct _HyScanRawData
{
  GObject parent_instance;

  HyScanRawDataPrivate *priv;
};

struct _HyScanRawDataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_raw_data_get_type                (void);

/**
 *
 * Функция создаёт новый объект обработки сырых данных. Если данные отсутствуют,
 * функция вернёт значение NULL.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param source_type тип источника данных;
 * \param source_channel индекс канала данных.
 *
 * \return Указатель на объект \link HyScanRawData \endlink или NULL.
 *
 */
HYSCAN_API
HyScanRawData         *hyscan_raw_data_new                     (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *track_name,
                                                                HyScanSourceType       source_type,
                                                                guint                  source_channel);

/**
 *
 * Функция создаёт новый объект обработки шумов сырых данных. Если данные отсутствуют,
 * функция вернёт значение NULL.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param source_type тип источника данных;
 * \param source_channel индекс канала данных.
 *
 * \return Указатель на объект \link HyScanRawData \endlink или NULL.
 *
 */
HYSCAN_API
HyScanRawData         *hyscan_raw_data_noise_new               (HyScanDB              *db,
                                                                const gchar           *project_name,
                                                                const gchar           *track_name,
                                                                HyScanSourceType       source_type,
                                                                guint                  source_channel);

/**
 *
 * Функция задаёт используемый кэш и префикс идентификаторов объектов для
 * хранения в нём.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink или NULL;
 * \param prefix префикс ключа кэширования или NULL.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_raw_data_set_cache               (HyScanRawData         *data,
                                                                HyScanCache           *cache,
                                                                const gchar           *prefix);

/**
 *
 * Функция возвращает информацию о местоположении приёмной антенны гидролокатора
 * в виде значения структуры \link HyScanAntennaPosition \endlink.
 *
 * \param data указатель на объект \link HyScanRawData \endlink.
 *
 * \return Местоположение приёмной антенны гидролокатора.
 *
 */
HYSCAN_API
HyScanAntennaPosition  hyscan_raw_data_get_position            (HyScanRawData         *data);

/**
 *
 * Функция возвращает параметры канала сырых данных в виде значения
 * структуры \link HyScanRawDataInfo \endlink.
 *
 * \param data указатель на объект \link HyScanRawData \endlink.
 *
 * \return Параметры канала сырых данных.
 *
 */
HYSCAN_API
HyScanRawDataInfo      hyscan_raw_data_get_info                (HyScanRawData         *data);

/**
 *
 * Функция возвращает тип источника данных.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 *
 * \return Тип источника данных из \link HyScanSourceType \endlink.
 *
 */
HYSCAN_API
HyScanSourceType       hyscan_raw_data_get_source              (HyScanRawData         *data);

/**
 *
 * Функция возвращает номер канала для этого канала данных.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 *
 * \return Номер канала.
 *
 */
HYSCAN_API
guint                  hyscan_raw_data_get_channel             (HyScanRawData         *data);

/**
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * \param data указатель на объект \link HyScanRawData \endlink.
 *
 * \return TRUE - если возможна запись данных, FALSE - если данные в режиме только чтения.
 *
 */
HYSCAN_API
gboolean               hyscan_raw_data_is_writable             (HyScanRawData         *data);

/**
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param first_index указатель на переменную для начального индекса или NULL;
 * \param last_index указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_raw_data_get_range               (HyScanRawData         *data,
                                                                guint32               *first_index,
                                                                guint32               *last_index);

/**
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return TRUE - если данные найдены, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
HyScanDBFindStatus     hyscan_raw_data_find_data               (HyScanRawData         *data,
                                                                gint64                 time,
                                                                guint32               *lindex,
                                                                guint32               *rindex,
                                                                gint64                *ltime,
                                                                gint64                *rtime);

/**
 *
 * Функция возвращает число точек данных для указанного индекса.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс данных.
 *
 * \return Число точек для указанного индекса или ноль в случае ошибки.
 *
 */
HYSCAN_API
guint32                hyscan_raw_data_get_values_count        (HyScanRawData         *data,
                                                                guint32                index);

/**
 *
 * Функция возвращает размер образа сигнала для указанного индекса.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс данных.
 *
 * \return Число точек для указанного индекса или ноль если свёртка не нужна.
 *
 */
HYSCAN_API
guint32                hyscan_raw_data_get_signal_size         (HyScanRawData         *data,
                                                                guint32                index);

/**
 *
 * Функция возвращает время приёма данных для указанного индекса.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс данных.
 *
 * \return Время приёма данных для указанного индекса или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint64                 hyscan_raw_data_get_time                (HyScanRawData         *data,
                                                                guint32                index);

/**
 *
 * Функция включает или отключает выполнения свёртки данных.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param convolve выполнять (TRUE) или нет (FALSE) свёртку данных.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_raw_data_set_convolve            (HyScanRawData         *data,
                                                                gboolean               convolve);

/**
 *
 * Функция возвращает образ сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения в переменную buffer_size будет записан действительный размер
 * образа в точках. Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен HyScanComplexFloat.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если образ успешно считан, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_raw_data_get_signal_image        (HyScanRawData         *data,
                                                                guint32                index,
                                                                HyScanComplexFloat    *buffer,
                                                                guint32               *buffer_size,
                                                                gint64                *time);

/**
 *
 * Функция возвращает значения коэффициентов усиления.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения в переменную buffer_size будет записано действительное число
 * коэффициентов усиления. Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен gfloat.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если коэффициенты успешно считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_raw_data_get_tvg_values          (HyScanRawData         *data,
                                                                guint32                index,
                                                                gfloat                *buffer,
                                                                guint32               *buffer_size,
                                                                gint64                *time);

/**
 *
 * Функция возвращает значения амплитуды сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер
 * считанных данных в точках. Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен gfloat.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_raw_data_get_amplitude_values    (HyScanRawData         *data,
                                                                guint32                index,
                                                                gfloat                *buffer,
                                                                guint32               *buffer_size,
                                                                gint64                *time);

/**
 *
 * Функция возвращает квадратурные отсчёты сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер
 * считанных данных в точках. Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен HyScanComplexFloat.
 *
 * \param data указатель на объект \link HyScanRawData \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_raw_data_get_quadrature_values   (HyScanRawData         *data,
                                                                guint32                index,
                                                                HyScanComplexFloat    *buffer,
                                                                guint32               *buffer_size,
                                                                gint64                *time);


G_END_DECLS

#endif /* __HYSCAN_RAW_DATA_H__ */

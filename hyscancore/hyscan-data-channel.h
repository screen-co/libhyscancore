/*!
 * \file hyscan-data-channel.h
 *
 * \brief Заголовочный файл класса первичной обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDataChannel HyScanDataChannel - класс первичной обработки акустических данных
 *
 * Класс HyScanDataChannel используется для первичной обработки акустических данных, таких как преобразование
 * типов данных, свёртки, вычисления аплитуды сигнала, его фазы или разности фаз между двумя приёмными каналами.
 *
 * Для создания объекта должны использоваться функции #hyscan_data_channel_create или #hyscan_data_channel_open.
 * В случае успешного создания канала данных или открытия существующего эти функции вернут указатель на
 * объект HyScanDataChannel.
 *
 * В своей работе класс HyScanDataChannel может использовать внешний кэш для хранения обработанных данных.
 * В случае запроса ранее обработанных данных пользователь получит их копию из кэша. Несколько экземпляров класса
 * HyScanDataChannel, обрабатывающих одинаковые каналы данных могут использовать один и тот же кэш. Таким образом,
 * данные обработанные в одной части приложения не потребуют повторной обработки в другой его части. Для корректной
 * работы системы кэширования, всем приложениям, требующим доступа к акустическим данным, рекомендуется использовать
 * этот объект, для их чтения и(или) записи.
 *
 * При работе с этим классом обычно возможны два сценария: записи данных и их чтения.
 *
 * Запись данных обычно производится программой управления гидролокатором или программой импортирующей данные от
 * внешних источников. Программы этого класса должны создать канал данных функцией #hyscan_data_channel_create и
 * записывать данные с помощью функции #hyscan_data_channel_add_data. Это приведёт к их первичной обработке с сохранением
 * результата в кэше. Если первичная обработка и кэширование результатов не требуется, можно не указывать объект
 * кэширования при создании объекта HyScanDataChannel.
 *
 * Программы, требующие только чтения записанных данных, должны использовать функцию #hyscan_data_channel_open. Это
 * требование относится в том числе и к программам записывающим данные, если они используют их в своей дальнейшей
 * работе, например для контроля записи или различных настроек.
 *
 * Параметры, определяемые структурой \link HyScanDataChannelInfo \endlink, используются при создании канала и в
 * дальнейшей обработке данных. Получить параметры записанных данных можно функцией #hyscan_data_channel_get_info.
 *
 * Функции работы с акустическими данными во многом схожи с функциями работы с каналами данных интерфейса \link HyScanDB \endlink.
 * Они также используют индексы и метки времени для навигации по данным.
 *
 * Функции #hyscan_data_channel_get_range и #hyscan_data_channel_find_data используются для определения границ записанных
 * данных и их поиска по метке времени. Эти функции аналогичны функциям \link hyscan_db_get_channel_data_range \endlink и
 * \link hyscan_db_find_channel_data \endlink интерфейса \link HyScanDB \endlink.
 *
 * Функция #hyscan_data_channel_add_data используется для записи данных в канал. Она доступна только если объект
 * HyScanDataChannel создан функцией #hyscan_data_channel_create.
 *
 * Для чтения данных используются следующие функции:
 *
 * - #hyscan_data_channel_get_amplitude_values - функция возвращает амплитуду акустического сигнала;
 * - #hyscan_data_channel_get_quadrature_values - функция возвращает квадратурные отсчёты акустического сигнала;
 * - #hyscan_data_channel_get_phase_values - функция возвращает фазу или разность фаз акустического сигнала;
 * - #hyscan_data_channel_get_raw_values - функция возвращает необработанные акустические данные в формате их записи.
 *
 * Все функции чтения данных, кроме #hyscan_data_channel_get_raw_values, используют в работе параметр buffer_size, определяющий
 * размер буфера для данных. Этот размер задаётся в числе точек данных способных поместиться в буфере, а не в байтах как
 * в интерфейсе \link HyScanDB \endlink. Размер буфера для функции #hyscan_data_channel_get_raw_values задаётся в байтах.
 *
 * Все функции чтения данных, кроме #hyscan_data_channel_get_raw_values, могут проводить обработку как с выполненим
 * свёртки сигнала с излучаемым образцом, так и без неё. Образец для свёртки считывается при создании объекта
 * автоматически. В случае необходимости образец сигнала для свёртки может быть задан функцией
 * #hyscan_data_channel_set_signal_image. Это может быть полезно при выполнении исследовательских работ.
 *
 * После выполнения первичной обработки, данные возвращаются в формате gfloat или HyScanComplexFloat.
 * Амплитудные значения находятся в пределах от нуля до единицы, квадратурные от минус единицы до единицы,
 * фазовые от минус Pi до Pi. Диапазоны значений данных возвращаемых функцией #hyscan_data_channel_get_raw_values
 * зависят от типа хранимых данных.
 *
 * Объект HyScanDataChannel необходимо удалить функцией g_object_unref после окончания работы.
 *
*/

#ifndef _hyscan_data_channel_h
#define _hyscan_data_channel_h

#include <hyscan-db.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS


#define G_TYPE_HYSCAN_DATA_CHANNEL             hyscan_data_channel_get_type()
#define HYSCAN_DATA_CHANNEL( obj )             ( G_TYPE_CHECK_INSTANCE_CAST ( ( obj ), G_TYPE_HYSCAN_DATA_CHANNEL, HyScanDataChannel ) )
#define HYSCAN_DATA_CHANNEL_CLASS( vtable )    ( G_TYPE_CHECK_CLASS_CAST ( ( vtable ), G_TYPE_HYSCAN_DATA_CHANNEL, HyScanDataChannelClass ) )
#define HYSCAN_DATA_CHANNEL_GET_CLASS( inst )  ( G_TYPE_INSTANCE_GET_INTERFACE ( ( inst ), G_TYPE_HYSCAN_DATA_CHANNEL, HyScanDataChannelClass ) )

GType hyscan_data_channel_get_type( void );


/*! \brief Информация о канале данных */
typedef struct HyScanDataChannelInfo {

  HyScanDataType             discretization_type;   /*!< Тип дискретизации данных в канале, \link HyScanDataType \endlink. */
  gfloat                discretization_frequency;   /*!< Частота дисретизации данных в канале, Гц. */

  HyScanSignalType           signal_type;           /*!< Тип сигнала, \link HyScanSignalType \endlink. */
  gfloat                     signal_frequency;      /*!< Рабочая (центральная) частота сигнала, Гц. */
  gfloat                     signal_spectrum;       /*!< Ширина спектра сигнала (дивиация в ЛЧМ и т.п.), Гц. */
  gfloat                     signal_duration;       /*!< Длительность сигнала, с. */

  gfloat                     antenna_x_offset;      /*!< Смещение от базовой точки поперёк оси движения судна (вправо '+', влево '-'), м. */
  gfloat                     antenna_y_offset;      /*!< Смещение от базовой точки вдоль оси движения судна (вперёд '+', назад '-'), м. */
  gfloat                     antenna_z_offset;      /*!< Вертикальное смещение от базовой точки (вверх '+', вниз '-'), м. */

  gfloat                     antenna_roll_offset;   /*!< Смещение по крену (вправо '+', влево '-'), градусы. */
  gfloat                     antenna_pitch_offset;  /*!< Смещение по дифференту (вперёд '+', назад '-'), градусы. */
  gfloat                     antenna_yaw_offset;    /*!< Смещение по курсу (влево '+',  вправо'-'), градусы. */

  gfloat                     antenna_base_offset;   /*!< Смещение антенны в блоке (база), м. */
  gfloat                     antenna_hpattern;      /*!< Горизонтальная ширина диаграммы направленности, градусы. */
  gfloat                     antenna_vpattern;      /*!< Вертикальная ширина диаграммы направленности, градусы. */

} HyScanDataChannelInfo;

typedef GObject HyScanDataChannel;
typedef GObjectClass HyScanDataChannelClass;


/*!
 *
 * Функция создаёт новый канал хранения акустических данных и открывает его для первичной обработки.
 * Параметры акустических данных определяются структурой \link HyScanDataChannelInfo \endlink. Образец сигнала
 * для свёртки задаётся как индекс записи канала данных с именем "signals". Создание и запись образца сигнала
 * должны быть выполнены заранее. \link HyScanSignals \endlink.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 * \param info параметры акустических данных;
 * \param signal_index индекс записи с образцом для свёртки.
 *
 * \return Указатель на объект \link HyScanDataChannel \endlink или NULL.
 *
*/
HyScanDataChannel *hyscan_data_channel_create( HyScanDB *db, const gchar *project_name, const gchar *track_name, const gchar *channel_name, HyScanDataChannelInfo *info, gint32 signal_index );


/*!
 *
 * Функция открывает существующий канал хранения акустических данных для первичной обработки.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 *
 * \return Указатель на объект \link HyScanDataChannel \endlink или NULL.
 *
*/
HyScanDataChannel *hyscan_data_channel_open( HyScanDB *db, const gchar *project_name, const gchar *track_name, const gchar *channel_name );


/*!
 *
 * Функция возвращает параметры акустических данных. Структуру \link HyScanDataChannelInfo \endlink необходимо
 * удалить функцией g_free после окончания работы.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 *
 * \return Указатель на структуру с параметрами акустических данных.
 *
*/
HyScanDataChannelInfo *hyscan_data_channel_get_info( HyScanDataChannel *dchannel );


/*!
 *
 * Функция задаёт новый образец сигнала для свёртки. Образец должен быть задан в комплексном виде и отнормирован.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param signal указатель на массив данных образца сигнала;
 * \param n_signal_points число точек образца сигнала.
 *
 * \return TRUE - если образец сигнала был изменён, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_set_signal_image( HyScanDataChannel *dchannel, HyScanComplexFloat *signal, gint32 n_signal_points );


/*!
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param first_index указатель на переменную для начального индекса или NULL;
 * \param last_index указатель на переменную для конечного индекса или NULL.
 *
 * \return TRUE - если границы записей определены, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_range( HyScanDataChannel *dchannel, gint32 *first_index, gint32 *last_index );


/*!
 *
 * Функция записывает новые данные в канал.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param time метка времени в микросекундах;
 * \param data указатель на записываемые данные;
 * \param size размер записываемых данных;
 * \param index указатель на переменную для сохранения индекса данных или NULL.
 *
 * \return TRUE - если данные успешно записаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_add_data( HyScanDataChannel *dchannel, gint64 time, gpointer data, gint32 size, gint32 *index );


/*!
 *
 * Функция возвращает значения амплитуды акустического сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер считанных данных в точках.
 * Размер считанных данных может быть ограничен размером буфера. Для того чтобы определить размер данных без
 * их чтения необходимо вызвать функцию с переменной buffer = NULL.
 *
 * Размер одной точки равен gfloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param convolve выполнять (TRUE) или нет (FALSE) свёртку акустических данных;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные или NULL;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_amplitude_values( HyScanDataChannel *dchannel, gboolean convolve, gint32 index, gfloat *buffer, gint32 *buffer_size, gint64 *time );


/*!
 *
 * Функция возвращает квадратурные отсчёты акустического сигнала.
 *
 * Значение параметра buffer_size аналогично функции #hyscan_data_channel_get_amplitude_values.
 * Размер одной точки равен HyScanComplexFloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param convolve выполнять (TRUE) или нет (FALSE) свёртку акустических данных;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные или NULL;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_quadrature_values( HyScanDataChannel *dchannel, gboolean convolve, gint32 index, HyScanComplexFloat *buffer, gint32 *buffer_size, gint64 *time );


/*!
 *
 * Функция возвращает фазу или разность фаз акустического сигнала.
 *
 * Если параметр pair содержит указатель на другой объект \link HyScanDataChannel \endlink вычисляется разность фаз
 * между текущим каналом и указанным в pair. Данные в парном канале считываются по метке времени совпадающей с данными
 * в текущем канале.
 *
 * Значение параметра buffer_size аналогично функции #hyscan_data_channel_get_amplitude_values.
 * Размер одной точки равен gfloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param pair указатель на объект \link HyScanDataChannel \endlink или NULL;
 * \param convolve выполнять (TRUE) или нет (FALSE) свёртку акустических данных;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные или NULL;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_phase_values( HyScanDataChannel *dchannel, HyScanDataChannel *pair, gboolean convolve, gint32 index, gfloat *buffer, gint32 *buffer_size, gint64 *time );


/*!
 *
 * Функция возвращает необработанные акустические данные в формате их записи.
 *
 * Значение параметра buffer_size аналогично функции \link hyscan_db_get_channel_data \endlink интерфейса \link HyScanDB \endlink.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные или NULL;
 * \param buffer_size указатель на переменную с размером области памяти для данных в байтах;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_raw_values( HyScanDataChannel *dchannel, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time );


/*!
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * Поведение функции аналогично \link hyscan_db_find_channel_data \endlink интерфейса \link HyScanDB \endlink.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param time искомый момент времени;
 * \param lindex указатель на переменную для сохранения "левого" индекса данных или NULL;
 * \param rindex указатель на переменную для сохранения "правого" индекса данных или NULL;
 * \param ltime указатель на переменную для сохранения "левой" метки времени данных или NULL;
 * \param rtime указатель на переменную для сохранения "правой" метки времени данных или NULL.
 *
 * \return TRUE - если данные найдены, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_find_data( HyScanDataChannel *dchannel, gint64 time, gint32 *lindex, gint32 *rindex, gint64 *ltime, gint64 *rtime );


G_END_DECLS

#endif // _hyscan_datachannel_h

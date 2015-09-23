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
 * типов данных, свёртки, вычисления аплитуды сигнала.
 *
 * Для создания объекта необходимо использовать функцию #hyscan_data_channel_new. При этом создаётся пустой объект
 * не связанный с каналом данных. Открыть существующий канал данных или создать новый можно функциями
 * #hyscan_data_channel_open и #hyscan_data_channel_create соответственно.
 *
 * В процессе работы можно закрыть обрабатываемый канал данных функцией #hyscan_data_channel_close и открыть
 * или создать другой канал данных.
 *
 * В своей работе класс HyScanDataChannel может использовать внешний кэш для хранения обработанных данных.
 * В случае запроса ранее обработанных данных пользователь получит их копию из кэша. Несколько экземпляров класса
 * HyScanDataChannel, обрабатывающих одинаковые каналы данных могут использовать один и тот же кэш. Таким образом,
 * данные обработанные в одной части приложения не потребуют повторной обработки в другой его части. Для корректной
 * работы системы кэширования, всем приложениям, требующим доступа к акустическим данным, рекомендуется использовать
 * этот объект, для их чтения и(или) записи. Кэш указывается при создании объекта.
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
 * Функция #hyscan_data_channel_add_data используется для записи данных в канал. Она доступна только если канал данных
 * создан функцией #hyscan_data_channel_create.
 *
 * Для чтения данных используются следующие функции:
 *
 * - #hyscan_data_channel_get_raw_values - функция возвращает необработанные акустические данные в формате их записи;
 * - #hyscan_data_channel_get_amplitude_values - функция возвращает амплитуду акустического сигнала;
 * - #hyscan_data_channel_get_quadrature_values - функция возвращает квадратурные отсчёты акустического сигнала.
 *
 * Все функции чтения данных, используют в работе параметр buffer_size, определяющий размер буфера для данных.
 * Для функции #hyscan_data_channel_get_raw_values размер буфера задаётся в байтах. А для функций
 * #hyscan_data_channel_get_amplitude_values и #hyscan_data_channel_get_quadrature_values этот размер задаёт
 * число точек данных способных поместиться в буфере
 *
 * Число точек данных для указанного индекса можно получить функцией #hyscan_data_channel_get_values_count.
 *
 * Все функции чтения данных, кроме #hyscan_data_channel_get_raw_values, могут проводить обработку как с выполненим
 * свёртки сигнала с образцом, так и без неё. Образец для свёртки считывается при создании объекта
 * автоматически. В случае необходимости образец сигнала для свёртки может быть задан функцией
 * #hyscan_data_channel_set_signal_image. Это может быть полезно при выполнении исследовательских работ.
 * Если образец сигнала считан автоматически или установлен пользователем свёртка будет выполняться по
 * умолчанию. Отключить свёртку или включить её снова можно функцией #hyscan_data_channel_set_convolve.
 *
 * После выполнения первичной обработки, данные возвращаются в формате gfloat или \link HyScanComplexFloat \endlink.
 * Амплитудные значения находятся в пределах от нуля до единицы, квадратурные от минус единицы до единицы. Диапазоны
 * значений данных возвращаемых функцией #hyscan_data_channel_get_raw_values зависят от типа хранимых данных.
 *
 * Объект HyScanDataChannel необходимо удалить функцией g_object_unref после окончания работы.
 *
*/

#ifndef _hyscan_data_channel_h
#define _hyscan_data_channel_h

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-types.h>

G_BEGIN_DECLS


#define HYSCAN_TYPE_DATA_CHANNEL                 ( hyscan_data_channel_get_type() )
#define HYSCAN_DATA_CHANNEL( obj )               ( G_TYPE_CHECK_INSTANCE_CAST( ( obj ), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannel ) )
#define HYSCAN_IS_DATA_CHANNEL( obj )            ( G_TYPE_CHECK_INSTANCE_TYPE( ( obj ), HYSCAN_TYPE_DATA_CHANNEL ) )
#define HYSCAN_DATA_CHANNEL_CLASS( klass )       ( G_TYPE_CHECK_CLASS_CAST( ( klass ), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannelClass ) )
#define HYSCAN_IS_DATA_CHANNEL_CLASS( klass )    ( G_TYPE_CHECK_CLASS_TYPE( ( klass ), HYSCAN_TYPE_DATA_CHANNEL ) )
#define HYSCAN_DATA_CHANNEL_GET_CLASS( obj )     ( G_TYPE_INSTANCE_GET_CLASS( ( obj ), HYSCAN_TYPE_DATA_CHANNEL, HyScanDataChannelClass ) )


/*! \brief Информация о канале данных */
typedef struct HyScanDataChannelInfo {

  HyScanDataType             discretization_type;   /*!< Тип дискретизации данных в канале, \link HyScanDataType \endlink. */
  gfloat                discretization_frequency;   /*!< Частота дискретизации данных в канале, Гц. */

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


GType hyscan_data_channel_get_type( void );


/*!
 *
 * Функция создаёт новый объект первичной обработки акустических данных.
 *
 * Если при создании объекта передан указатель на интерфейс системы кэширования, она будет
 * использоваться. Данные кэшируются по ключу вида "cache_prefix.project.track.channel.type.index", где
 *
 * - cache_prefix - опциональный префикс ключа или NULL;
 * - project - имя проекта;
 * - track - имя галса;
 * - channel - имя канала данных;
 * - type - тип обработки данных;
 * - index - индекс данных.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param cache_prefix префикс ключа кэширования или NULL.
 *
 * \return Указатель на объект \link HyScanDataChannel \endlink.
 *
*/
HyScanDataChannel *hyscan_data_channel_new( HyScanDB *db, HyScanCache *cache, const gchar *cache_prefix );


/*!
 *
 * Функция создаёт новый канал хранения акустических данных и открывает его для первичной обработки.
 * Параметры акустических данных определяются структурой \link HyScanDataChannelInfo \endlink. Образец сигнала
 * для свёртки задаётся как индекс записи канала данных с именем "signals". Создание и запись образца сигнала
 * должны быть выполнены заранее. \link !!!HyScanSignals!!! \endlink.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 * \param info параметры акустических данных;
 * \param signal_index индекс записи с образцом для свёртки.
 *
 * \return TRUE - если канал данных был создан и открыт для обработки, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_create( HyScanDataChannel *dchannel, const gchar *project_name, const gchar *track_name, const gchar *channel_name, HyScanDataChannelInfo *info, gint32 signal_index );


/*!
 *
 * Функция открывает существующий канал хранения акустических данных для первичной обработки.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param project_name название проекта;
 * \param track_name название галса;
 * \param channel_name название канала данных;
 *
 * \return TRUE - если канал данных был открыт для обработки, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_open( HyScanDataChannel *dchannel, const gchar *project_name, const gchar *track_name, const gchar *channel_name );


/*!
 *
 * Функция закрывает текущий обрабатываемый канал данных.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink.
 *
 * \return Нет.
 *
*/
void hyscan_data_channel_close( HyScanDataChannel *dchannel );


/*!
 *
 * Функция возвращает параметры акустических данных. Структуру \link HyScanDataChannelInfo \endlink необходимо
 * удалить функцией g_free после окончания использования.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink.
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
 * Функция изменяет необходимость выполнения свёртки акустических данных.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param convolve выполнять (TRUE) или нет (FALSE) свёртку акустических данных.
 *
 * \return Нет.
 *
*/
void hyscan_data_channel_set_convolve( HyScanDataChannel *dchannel, gboolean convolve );


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
 * Функция возвращает число точек данных для указанного индекса.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс данных.
 *
 * \return Число точек для указанного индекса или отрицательное число в случае ошибки.
 *
*/
gint32 hyscan_data_channel_get_values_count( HyScanDataChannel *dchannel, gint32 index );


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
 * Функция возвращает необработанные акустические данные в формате их записи.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в байтах.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер считанных данных в байтах.
 * Размер считанных данных может быть ограничен размером буфера.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в байтах;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_raw_values( HyScanDataChannel *dchannel, gint32 index, gpointer buffer, gint32 *buffer_size, gint64 *time );


/*!
 *
 * Функция возвращает значения амплитуды акустического сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер считанных данных в точках.
 * Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен gfloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_amplitude_values( HyScanDataChannel *dchannel, gint32 index, gfloat *buffer, gint32 *buffer_size, gint64 *time );


/*!
 *
 * Функция возвращает квадратурные отсчёты акустического сигнала.
 *
 * Перед вызовом функции в переменную buffer_size должен быть записан размер буфера в точках.
 * После успешного чтения данных в переменную buffer_size будет записан действительный размер считанных данных в точках.
 * Размер считанных данных может быть ограничен размером буфера.
 *
 * Размер одной точки равен HyScanComplexFloat.
 *
 * \param dchannel указатель на объект \link HyScanDataChannel \endlink;
 * \param index индекс считываемых данных;
 * \param buffer указатель на область памяти в которую считываются данные;
 * \param buffer_size указатель на переменную с размером области памяти для данных в точках;
 * \param time указатель на переменную для сохранения метки времени считанных данных или NULL.
 *
 * \return TRUE - если данные успешно считаны и обработаны, FALSE - в случае ошибки.
 *
*/
gboolean hyscan_data_channel_get_quadrature_values( HyScanDataChannel *dchannel, gint32 index, HyScanComplexFloat *buffer, gint32 *buffer_size, gint64 *time );


G_END_DECLS

#endif // _hyscan_datachannel_h

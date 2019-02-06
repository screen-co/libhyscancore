/**
 * \file hyscan-forward-look-player.h
 *
 * \brief Заголовочный файл класса управления просмотром данных вперёдсмотрящего локатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanForwardLookPlayer HyScanForwardLookPlayer - класс управления просмотром данных вперёдсмотрящего локатора
 *
 * Класс HyScanForwardLookPlayer используется для воспроизведения данных вперёдсмотрящего
 * гидролокатора. Класс обрабатывает данные в фоновом потоке с помощью класса
 * \link HyScanForwardLookData \endlink и при их готовности к отображению посылает сигнал.
 * Отправка сигналов осуществляется из основного цикла GMainLoop. Все функции класса работают
 * в неблокирующем режиме и являются потокобезопасными. Класс может использоваться
 * из основного цикла Gtk.
 *
 * Возможны два режима работы: реальное время и воспроизведение записанных данных.
 * В режиме реального времени класс обрабатывает строки по мере их записи. При
 * обработке уже записанных данных возможно воспроизведение с различной скоростью
 * как в прямом, так и в обратном направлении.
 *
 * Объект создаётся функцией #hyscan_forward_look_player_new.
 *
 * При воспроизведении класс пытается отправлять обработанные данные со скоростью
 * их последующего отображения, определяемой частотой кадров в секунду. При этом возможны
 * пропуски некоторых строк, для поддержкания требуемой скорости воспроизведения. Целевой
 * уровень числа кадров в секунду задаётся функцией #hyscan_forward_look_player_set_fps.
 *
 * Для корректной обработки данных необходимо точное указание скорости звука, для
 * этого предназначена функция #hyscan_forward_look_player_set_sv.
 *
 * Открытие и закрытие галсов производится функциями #hyscan_forward_look_player_open
 * и #hyscan_forward_look_player_close соответственно.
 *
 * Режим отображения в реальном времени включается функцией #hyscan_forward_look_player_real_time.
 *
 * Активация режиме воспроизведения и управление воспроизведением осуществляется функциями
 * #hyscan_forward_look_player_play, #hyscan_forward_look_player_pause,
 * #hyscan_forward_look_player_stop и #hyscan_forward_look_player_seek.
 *
 * При готовности данных отправлются сигналы:
 *
 * - "range" - при изменении диапазона индексов строк данных;
 * - "data" - при завершении обработки текущей строки данных.
 *
 * Прототипы обработчиков этих сигналов:
 *
 * \code
 *
 * void    range_cb  (HyScanForwardLookPlayer     *player,
 *                    guint32                      first_index,
 *                    guint32                      last_index,
 *                    gpointer                     user_data);
 *
 * \endcode
 *
 * Где first_index, last_index начальный и конечный индексы строк данных текущего
 * обрабатываемого галса.
 *
 * \code
 *
 * void    data_cb   (HyScanForwardLookPlayer     *player,
 *                    HyScanForwardLookPlayerInfo *info,
 *                    HyScanAntennaOffset         *offset,
 *                    HyScanForwardLookDOA        *doa,
 *                    guint32                      n_points,
 *                    gpointer                     user_data);
 *
 * \endcode
 *
 * Где:
 * - info - информация о текущем зондировании \link HyScanForwardLookPlayerInfo \endlink;
 * - offset - смещение антенны локатора \link HyScanAntennaOffset \endlink;
 * - doa - массив точек целей \link HyScanForwardLookDOA \endlink;
 * - n_points - число точек целей.
 *
 */

#ifndef __HYSCAN_FORWARD_LOOK_PLAYER_H__
#define __HYSCAN_FORWARD_LOOK_PLAYER_H__

#include <hyscan-forward-look-data.h>

G_BEGIN_DECLS

/** \brief Информация о текущем зондировании вперёд смотрящего локатора. */
typedef struct
{
  guint                index;                  /**< Индекс строки зондирования. */
  gint64               time;                   /**< Время приёма строки зондирования. */
  gdouble              alpha;                  /**< Сектор обзора локатора в горизонтальной плоскости [-alpha +alpha], рад. */
  gdouble              distance;               /**< Максимальная дистанция обзора, м. */
} HyScanForwardLookPlayerInfo;

#define HYSCAN_TYPE_FORWARD_LOOK_PLAYER             (hyscan_forward_look_player_get_type ())
#define HYSCAN_FORWARD_LOOK_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_FORWARD_LOOK_PLAYER, HyScanForwardLookPlayer))
#define HYSCAN_IS_FORWARD_LOOK_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_FORWARD_LOOK_PLAYER))
#define HYSCAN_FORWARD_LOOK_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_FORWARD_LOOK_PLAYER, HyScanForwardLookPlayerClass))
#define HYSCAN_IS_FORWARD_LOOK_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_FORWARD_LOOK_PLAYER))
#define HYSCAN_FORWARD_LOOK_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_FORWARD_LOOK_PLAYER, HyScanForwardLookPlayerClass))

typedef struct _HyScanForwardLookPlayer HyScanForwardLookPlayer;
typedef struct _HyScanForwardLookPlayerPrivate HyScanForwardLookPlayerPrivate;
typedef struct _HyScanForwardLookPlayerClass HyScanForwardLookPlayerClass;

struct _HyScanForwardLookPlayer
{
  GObject parent_instance;

  HyScanForwardLookPlayerPrivate *priv;
};

struct _HyScanForwardLookPlayerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_forward_look_player_get_type     (void);

/**
 *
 * Функция создаёт новый объект обработки и воспроизведения данных
 * вперёдсмотрящего локатора.
 *
 * \return Указатель на объект \link HyScanForwardLookPlayer \endlink.
 *
 */
HYSCAN_API
HyScanForwardLookPlayer       *hyscan_forward_look_player_new          (void);

/**
 *
 * Функция устанавливает целевой уровень числа отображаемых кадров в секунду.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink;
 * \param fps число кадров в секунду от 1 до 100.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_set_fps      (HyScanForwardLookPlayer       *player,
                                                                        guint                          fps);

/**
 *
 * Функция задаёт скорость звука в воде, используемую для обработки данных.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink;
 * \param sound_velocity скорость звука, м/с.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_set_sv       (HyScanForwardLookPlayer       *player,
                                                                        gdouble                        sound_velocity);

/**
 *
 * Функция открывает галс для обработки и воспроизведения. При этом
 * текущая позиция воспроизведения устанавливается в начало галса.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink;
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project_name название проекта;
 * \param track_name название галса.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_open         (HyScanForwardLookPlayer       *player,
                                                                        HyScanDB                      *db,
                                                                        HyScanCache                   *cache,
                                                                        const gchar                   *project_name,
                                                                        const gchar                   *track_name);

/**
 *
 * Функция закрывает текущий галс.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_close        (HyScanForwardLookPlayer       *player);

/**
 *
 * Функция включает режим отображения данных в реальном времени. По мере
 * записи новых данных они будут обрабатываться и отправляться пользователю.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_real_time    (HyScanForwardLookPlayer       *player);

/**
 *
 * Функция включает режим воспроизведения записанных данных. Воспроизведение
 * начинается с текущей позиции. Скорость воспроизведения определяет коэффициент
 * замедления (< 1.0) или ускорения (> 1.0) течения времени. Если скорость
 * воспроизведения отрицательная, проигрывание осуществляется в обратном направлении.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink;
 * \param speed скорость воспроизведения.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_play         (HyScanForwardLookPlayer       *player,
                                                                        gdouble                        speed);

/**
 *
 * Функция приостанавливает воспроизведение записанных данных. При этом текущая
 * позиция воспроизведения не изменяется.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_pause        (HyScanForwardLookPlayer       *player);

/**
 *
 * Функция останавливает воспроизведение записанных данных. При этом текущая
 * позиция воспроизведения перемещается в начало галса.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_stop         (HyScanForwardLookPlayer       *player);

/**
 *
 * Функция перемещает текущую позицию воспроизведения в указанное место.
 *
 * \param player указатель на объект \link HyScanForwardLookPlayer \endlink;
 * \param index новая позиция воспроизведения.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_forward_look_player_seek         (HyScanForwardLookPlayer       *player,
                                                                        guint32                        index);

G_END_DECLS

#endif /* __HYSCAN_FORWARD_LOOK_PLAYER_H__ */

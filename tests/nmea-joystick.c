/* nmea-joystick.c
 *
 * https://gist.github.com/jasonwhite/c5b2048c15993d285130
 * Copyright Jason White
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <hyscan-geo.h>
#include <math.h>
#include <gio/gio.h>
#include <string.h>

#define METER_PER_SECOND_TO_KNOTS 1.94384 /* Коэффициент перевода из метров в секунды в узлы. */
#define UPDATE_INTERVAL           50      /* Период пересчёта положения судна, мс. */
#define HISTORY_SIZE              100     /* Размер буфер с историей положения судна. */
#define MAX_ACCELERATION          20.0    /* Максимальное ускорение, м/с/c/. */
#define ACCELERATION_CHNG_SPD     5.0     /* Скорость изменения ускорения, м/с/c/c. */

/* Приводит угол к границам [0, 2 * G_PI) */
#define FIT_ANGLE(x) ((x) < 0 ? (x) + G_PI * 2 : ((x) >= G_PI * 2 ? (x) - G_PI * 2 : (x)))

/* Положение осей джостика. */
typedef struct
{
  gdouble              x;               /* Положение джойстика по оси X. */
  gdouble              y;               /* Положение джойстика по оси Y. */
} JoystickState;

/* Положение судна относительно точки отсчёта на земле. */
typedef struct
{
  gint64               time;            /* Время, для которого рассчитано положение, мкс. */
  HyScanGeoCartesian2D position;        /* Координаты (X, Y), м. */
  gdouble              vx;              /* Скорость по оси X, м/c. */
  gdouble              vy;              /* Скорость по оси Y, м/c. */
  gdouble              heading;         /* Курс, радианы. */
} VesselPos;

/* Параметры для рассчёта положения судна. */
typedef struct
{
  gint64               time;            /* Время, для которого рассчитано положение, мкс. */
  HyScanGeoCartesian2D position;        /* Координаты (X, Y), м. */
  gdouble              accel;           /* Текущее ускорение. */
  gdouble              speed;           /* Скорость двигателя, м/с. */
  gdouble              v_angle;         /* Направление скорости двигателя, рад. */
  gdouble              vx;              /* Скорость судна относительно воды по оси X, м/с. */
  gdouble              vy;              /* Скорость судна относительно воды по оси Y, м/с. */
  gdouble              flow_vx;         /* Скорость течения по оси X, м/с. */
  gdouble              flow_vy;         /* Скорость течения по оси Y, м/с. */
} VesselState;


static gint freq = 10;                  /* Частота выдачи NMEA-данных, Гц. */
gboolean hdt_off = FALSE;               /* Отключен вывод HDT-сообщений. */
gboolean rmc_off = FALSE;               /* Отключен вывод RMC-сообщений. */
static gdouble slat = 55.0;             /* Широта начальной точки. */
static gdouble slon = 33.0;             /* Долгота начальной точки. */
static gint axis_accel = 0;             /* Ось джойстика для считывания ускорения, Y. */
static gint axis_steer = 0;             /* Ось джойстика для считывания поворота, X. */
static gint btn_accel = -1;             /* Кнопка ускорения, -1 для отключения. */

static HyScanGeo *geo;                  /* Объект перевода декартовых координат в географические. */
static GMutex mutex;                    /* Мьютекс для доступа к joystick_state. */
static JoystickState joystick_state;    /* Состояние джойстика. */
static VesselState vessel;              /* Параметры движения судна. */

static VesselPos history[HISTORY_SIZE]; /* Кольцевой буфер с историей положения судна. */
static guint history_idx;               /* Индекс текущего элемента. */
static guint history_delay;             /* Число индексов, на которых запаздывает выдача данных. */

/**
 * Reads a joystick event from the joystick device.
 *
 * Returns 0 on success. Otherwise -1 is returned.
 */
int read_event(int fd, struct js_event *event)
{
    ssize_t bytes;

    bytes = read(fd, event, sizeof(*event));

    if (bytes == sizeof(*event))
        return 0;

    /* Error, could not read full event. */
    return -1;
}

/**
 * Returns the number of axes on the controller or 0 if an error occurs.
 */
size_t get_axis_count(int fd)
{
    __u8 axes;

    if (ioctl(fd, JSIOCGAXES, &axes) == -1)
        return 0;

    return axes;
}

/**
 * Returns the number of buttons on the controller or 0 if an error occurs.
 */
size_t get_button_count(int fd)
{
    __u8 buttons;
    if (ioctl(fd, JSIOCGBUTTONS, &buttons) == -1)
        return 0;

    return buttons;
}

/**
 * Current state of an axis.
 */
struct axis_state {
    short x, y;
};

/**
 * Keeps track of the current axis state.
 *
 * NOTE: This function assumes that axes are numbered starting from 0, and that
 * the X axis is an even number, and the Y axis is an odd number. However, this
 * is usually a safe assumption.
 *
 * Returns the axis that the event indicated.
 */
size_t get_axis_state(struct js_event *event, struct axis_state axes[3])
{
    size_t axis = event->number / 2;

    if (axis < 3)
    {
        if (event->number % 2 == 0)
            axes[axis].x = event->value;
        else
            axes[axis].y = event->value;
    }

    return axis;
}

/* Возвращает состояние с указанной задержкой. */
static inline void
history_get (VesselPos *state_c)
{
  guint index;

  index = (history_idx + HISTORY_SIZE - history_delay) % HISTORY_SIZE;
  *state_c = history[index];
}

/* Добавляет состояние в историю. */
static inline void
history_push (VesselPos pos)
{
  history_idx = (history_idx + 1) % HISTORY_SIZE;
  history[history_idx] = pos;
}

static gchar *
nmea_wrap (const gchar *inner)
{
  gint checksum = 0;
  const gchar *ch;
  gchar *sentence;

  /* Подсчитываем чек-сумму. */
  for (ch = inner; *ch != '\0'; ch++)
    checksum ^= *ch;

  sentence = g_strdup_printf ("$%s*%02X\r", inner, checksum);

  return sentence;
}

/* Отправляем RMC-строку по UDP. */
static gboolean
send_rmc (gpointer user_data)
{
  static gchar *inner = NULL;
  gsize inner_len = 300;
  gchar *sentence;
  gdouble cur_time;
  HyScanGeoGeodetic coord;
  VesselPos state_c;
  GSocket *socket = G_SOCKET (user_data);

  gint hour, min;
  gdouble sec;
  gint lat, lon;
  gdouble lat_min, lon_min;
  gdouble speed, track;
  gboolean north, east;
  const gchar *date = "191119";

  if (inner == NULL)
    inner = g_new0 (gchar, inner_len);

  history_get (&state_c);
  g_debug ("Delay: %f\n", (g_get_monotonic_time () - state_c.time) * 1e-6);

  hyscan_geo_topoXY2geo (geo, &coord, state_c.position, 0);

  cur_time = state_c.time * 1e-6;
  hour = cur_time / 3600;
  cur_time -= hour * 3600;

  min = cur_time / 60;
  cur_time -= min * 60;

  sec = cur_time;

  north = coord.lat > 0;
  east = coord.lon > 0;
  coord.lat = fabs (coord.lat);
  coord.lon = fabs (coord.lon);
  lat = coord.lat;
  lat_min = (coord.lat - lat) * 60;
  lon = coord.lon;
  lon_min = (coord.lon - lon) * 60;

  speed = hypot (state_c.vx, state_c.vy) * METER_PER_SECOND_TO_KNOTS;
  track = -atan2 (state_c.vy, state_c.vx) / G_PI * 180.0;
  if (track < 0.0)
    track += 360.0;
  else if (track == -0.0)  /* Меняем "-0" на "0". */
    track = 0.0;

  if (!rmc_off)
    {
      g_snprintf (inner, inner_len,
                  "GPRMC,%02d%02d%05.2f,A,"
                  "%02d%08.5f,%c,%03d%08.5f,%c,"
                  "%05.1f,%05.1f,"
                  "%s,011.5,E",
                  hour, min, sec,
                  lat, lat_min, north ? 'N' : 'S', lon, lon_min, east ? 'E' : 'W',
                  speed, track,
                  date);

      sentence = nmea_wrap (inner);
      g_socket_send (socket, sentence, strlen (sentence), NULL, NULL);
      g_free (sentence);
    }

  g_snprintf (inner, inner_len,
              "GPGGA,%02d%02d%05.2f,"
              "%02d%08.5f,%c,%03d%08.5f,%c,"
              "2,6,1.2,18.893,M,-25.669,M,2.0,0031",
              hour, min, sec,
              lat, lat_min, north ? 'N' : 'S', lon, lon_min, east ? 'E' : 'W');
  sentence = nmea_wrap (inner);
  g_socket_send (socket, sentence, strlen (sentence), NULL, NULL);
  g_free (sentence);

  if (!hdt_off)
    {
      g_snprintf (inner, inner_len, "GPHDT,%.2f,T", state_c.heading / G_PI * 180.0);
      sentence = nmea_wrap (inner);
      g_socket_send (socket, sentence, strlen (sentence), NULL, NULL);
      g_free (sentence);
    }

  return G_SOURCE_CONTINUE;
}

/* Считывает состояние джойстика. */
static gpointer
joystick_read (gchar *device)
{
  struct js_event event;
  struct axis_state axes[3] = {0};
  size_t axis;
  int js;

  js = open (device, O_RDONLY);
  if (js == -1)
    g_error ("Could not open device: %s", device);

  while (read_event(js, &event) == 0)
    {
      switch (event.type)
      {
        case JS_EVENT_BUTTON:
          printf ("Button %u %s\n", event.number, event.value ? "pressed" : "released");
          if (btn_accel == event.number)
            {
              g_mutex_lock (&mutex);
              joystick_state.y = event.value ? -1.0 : 0.0;
              g_mutex_unlock (&mutex);
            }
          break;

        case JS_EVENT_AXIS:
          axis = get_axis_state (&event, axes);
          if (axis < 3)
            printf ("Axis %zu at (%6d, %6d)\n", axis, axes[axis].x, axes[axis].y);

          g_mutex_lock (&mutex);
          if ((gint) axis == axis_steer)
            joystick_state.x = axes[axis].x / 32768.0;

          if (btn_accel == -1 && (gint) axis == axis_accel)
            joystick_state.y = axes[axis].y / 32768.0;
          g_mutex_unlock (&mutex);

          break;

        default:
          /* Ignore init events. */
          break;
      }

      fflush(stdout);
    }

  close (js);

  return NULL;
}

/* Вычисляет положение судна. */
static gboolean
update_position ()
{
  gint64 cur_time;
  gdouble dt;
  JoystickState js_copy;

  cur_time = g_get_monotonic_time ();

  g_mutex_lock (&mutex);
  js_copy = joystick_state;
  g_mutex_unlock (&mutex);

  dt = 1e-6 * (gdouble) (cur_time - vessel.time);

  /* Ускорение двигателя. */
  vessel.v_angle += -js_copy.x * G_PI_4 * dt;
  vessel.accel += - ACCELERATION_CHNG_SPD * js_copy.y * dt;
  vessel.accel = CLAMP (vessel.accel, 0, MAX_ACCELERATION);
  vessel.speed += vessel.accel * dt;

  /* Сопротивление воды. */
  {
    gdouble resistance;
    gdouble speed2;

    speed2 = vessel.speed * vessel.speed;
    resistance =  speed2 * (1 + speed2 / (186 * 186)) * 0.3 * dt;
    if (vessel.speed > resistance)
      vessel.speed -= resistance;
    else if (vessel.speed < -resistance)
      vessel.speed += resistance;
    else
      vessel.speed = 0.0;
  }

  /* Скорость по компонентам. */
  vessel.vx = vessel.speed * cos (vessel.v_angle);
  vessel.vy = vessel.speed * sin (vessel.v_angle);
  vessel.v_angle = FIT_ANGLE (vessel.v_angle);

  /* Снос течением. */
  gdouble speed_x, speed_y;

  speed_x = vessel.vx + vessel.flow_vx;
  speed_y = vessel.vy + vessel.flow_vy;
  vessel.position.x += speed_x * dt;
  vessel.position.y += speed_y * dt;
  vessel.time = cur_time;

  /* Записываем итоговое значение. */
  {
    VesselPos final_pos;

    final_pos.time = cur_time;
    final_pos.vx = speed_x;
    final_pos.vy = speed_y;
    final_pos.position = vessel.position;
    final_pos.heading = FIT_ANGLE (G_PI * 2 - vessel.v_angle);

    history_push (final_pos);
  }

  return G_SOURCE_CONTINUE;
}

/* Подключение к UDP сокету. */
static GSocket *
connect_udp (const gchar *host,
             guint        port)
{
  GSocketAddress *address;
  GSocket *my_socket = NULL;
  GError *error = NULL;
  GSocketFamily family;

  address = g_inet_socket_address_new_from_string (host, port);
  if (address == NULL)
    g_error ("Unknown host address: %s:%d", host, port);

  family = g_socket_address_get_family (address);
  my_socket = g_socket_new (family,
                            G_SOCKET_TYPE_DATAGRAM,
                            G_SOCKET_PROTOCOL_UDP,
                            &error);
  if (my_socket == NULL)
    g_error ("Couldn't create socket: %s", error->message);

  if (!g_socket_connect (my_socket, address, NULL, NULL))
    g_error ("Couldn't connect to %s:%d", host, port);

  return my_socket;
}

int main (int argc, char *argv[])
{
  GThread *joystick_thread;
  GSocket *socket;

  gint delay_ms = 0;
  gchar *host = NULL;
  gint port = 0;
  gdouble flow_dir = 0;
  gdouble flow_rate = 0;
  static gchar *device = NULL;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "device",    'd', 0, G_OPTION_ARG_STRING, &device,     "Joystick device (/dev/input/js0)", NULL },
        { "axis-rot",   0 , 0, G_OPTION_ARG_INT,    &axis_steer, "Steer axis number",                NULL},
        { "axis-accel", 0 , 0, G_OPTION_ARG_INT,    &axis_accel, "Accelerate axis number",           NULL},
        { "btn-accel",  0 , 0, G_OPTION_ARG_INT,    &btn_accel,  "Accelerate button number",         NULL},

        { "host",      'h', 0, G_OPTION_ARG_STRING, &host,       "Destination IP address",           NULL },
        { "port",      'p', 0, G_OPTION_ARG_INT,    &port,       "Destination UDP port",             NULL },

        { "freq",      'f', 0, G_OPTION_ARG_INT,    &freq,       "NMEA frequency, Hz (default 10)",  NULL },
        { "hdt-off",    0 , 0, G_OPTION_ARG_NONE,   &hdt_off,    "Disable HDT messages",             NULL },
        { "rmc-off",    0 , 0, G_OPTION_ARG_NONE,   &rmc_off,    "Disable RMC messages",             NULL },
        { "delay",      0 , 0, G_OPTION_ARG_INT,    &delay_ms,   "Add delay to NMEA sentences, ms",  NULL },

        { "slat",      'n', 0, G_OPTION_ARG_DOUBLE, &slat,       "Start latitude",                   NULL },
        { "slon",      'e', 0, G_OPTION_ARG_DOUBLE, &slon,       "Start longitude",                  NULL },
        { "flow-rate",  0 , 0, G_OPTION_ARG_DOUBLE, &flow_rate,  "Flow rate, m/s",                   NULL },
        { "flow-dir",   0 , 0, G_OPTION_ARG_DOUBLE, &flow_dir,   "Flow direction",                   NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_description (context, "Program emulates boat motion on water. "
                                               "Program reads input from joystick, calculates boat position and speed, "
                                               "and sends data in NMEA RMC-sentences using UDP.");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (device == NULL)
      device = "/dev/input/js0";

    /* Переводим задержку из милисекунд в индексы. */
    history_delay = CLAMP ((delay_ms / UPDATE_INTERVAL), 0, HISTORY_SIZE - 1);

    if ((host == NULL) || (port < 1024) || (port > 65535) || (freq <= 0 || freq > 1000))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  /* Подключаем джойстик и читаем его в отдельном потоке. */
  joystick_thread = g_thread_new ("joystick-read", (GThreadFunc) joystick_read, device);

  /* Подключаемся к UDP-сокету. */
  socket = connect_udp (host, port);

  /* Географическая система координат. */
  {
    HyScanGeoGeodetic origin;
    origin.lat = slat;
    origin.lon = slon;
    origin.h = 0;
    geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
  }

  /* Начальное состояние судна. */
  vessel.time = g_get_monotonic_time();
  vessel.flow_vx = flow_rate * sin (flow_dir / 180 * G_PI);
  vessel.flow_vy = flow_rate * cos (flow_dir / 180 * G_PI);

  /* Main loop. */
  {
    GMainLoop *loop;

    loop = g_main_loop_new (NULL, FALSE);
    g_timeout_add (UPDATE_INTERVAL, update_position, NULL);
    g_timeout_add (1000 / freq, send_rmc, socket);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
  }

  g_thread_join (joystick_thread);
  g_object_unref (geo);
  g_object_unref (socket);

  return 0;
}

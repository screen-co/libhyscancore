#include <hyscan-convolution.h>

#include <glib/gstdio.h>
#include <string.h>
#include <math.h>

int
main (int argc, char **argv)
{
  gdouble frequency = 0.0;        /* Частота сигнала. */
  gdouble duration = 0.0;         /* Длительность сигнала. */
  gdouble discretization = 0.0;   /* Частота дискретизации. */

  HyScanConvolution *convolution;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "discretization", 'd', 0, G_OPTION_ARG_DOUBLE, &discretization, "Signal discretization, Hz", NULL },
        { "frequency", 'f', 0, G_OPTION_ARG_DOUBLE, &frequency, "Signal frequency, Hz", NULL },
        { "duration", 't', 0, G_OPTION_ARG_DOUBLE, &duration, "Signal duration, s", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message (error->message);
        return -1;
      }

    if (discretization < 1.0 || frequency < 1.0 || duration < 1e-7)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
  }

  /* Объект выполнения свёртки. */
  convolution = hyscan_convolution_new ();

  /* Создаём образец сигнала для свёртки. */
  {
    guint32 signal_size = discretization * duration;
    HyScanComplexFloat *signal = g_malloc (signal_size * sizeof(HyScanComplexFloat));
    guint32 i;

    g_message( "signal size = %d", signal_size);

    /* Образец сигнала. */
    for (i = 0; i < signal_size; i++)
      {
        gdouble time = (1.0 / discretization) * i;
        gdouble phase = 2.0 * G_PI * frequency * time;
        signal[i].re = cos (phase);
        signal[i].im = sin (phase);
      }

    hyscan_convolution_set_image (convolution, signal, signal_size);

    g_free (signal);
  }

  /* Тестовые данные для проверки свёртки. Массив размером 100 * signal_size.
     Сигнал располагается со смещением в две длительности. Все остальные индексы
     массива заполнены нулями. Используется тональный сигнал. */
  {
    gint32 signal_size = discretization * duration;
    gint32 data_size = 100 * signal_size;
    HyScanComplexFloat *data;
    gfloat *amplitude;
    gdouble delta = 0.0;
    gint32 i, j;

    data = g_malloc (data_size * sizeof(HyScanComplexFloat));
    amplitude = g_malloc (data_size * sizeof(gfloat));

    /* Данные для свёртки. */
    memset (data, 0, data_size * sizeof(HyScanComplexFloat));
    for (i = 2 * signal_size; i < 3 * signal_size; i++)
      {
        gdouble time = (1.0 / discretization) * i;
        gdouble phase = 2.0 * G_PI * frequency * time;
        data[i].re = cos (phase);
        data[i].im = sin (phase);
      }

    /* Выполняем свёртку. */
    hyscan_convolution_convolve (convolution, data, data_size);

    /* Для тонального сигнала проверяем, что его свёртка совпадает с треугольником,
       начинающимся с signal_size, пиком на 2 * signal_size и спадающим до 3 * signal_size. */
    memset (amplitude, 0, data_size * sizeof(gfloat));
    for (i = signal_size, j = 0; j < signal_size; i++, j++)
      amplitude[i] = (gfloat) j / signal_size;
    for (i = 2 * signal_size, j = 0; j < signal_size; i++, j++)
      amplitude[i] = 1.0 - (gfloat) j / signal_size;

    for (i = 0; i < data_size; i++)
      {
        gfloat a = sqrtf (data[i].re * data[i].re + data[i].im * data[i].im);
        delta += fabs (amplitude[i] - a);
      }

    g_message( "full amplitude error = %f", delta);
    g_message( "mean amplitude error = %f", delta / data_size);

    g_free (amplitude);
    g_free (data);
  }

  /* Удаляем объект свёртки. */
  g_object_unref (convolution);

  return 0;
}

#include <hyscan-nav-smooth.h>

/* HyScanNavDataDummy - простая и неполная реализация HyScanNavData просто для тестирования. */
#define HYSCAN_TYPE_NAV_DATA_DUMMY    (hyscan_nav_data_dummy_get_type ())
#define HYSCAN_NAV_DATA_DUMMY(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_DATA_DUMMY, HyScanNavDataDummy))

static gint64  times[]     = { 1000, 1200, 1400, 1600, 1800, 2000 }; /* Метки времени. */
static gdouble values[]    = {   10,   20,   40,  355,    5,   10 }; /* Значения навигационных данных. */
static guint32 first_index = 100;                                    /* Индекс первого элемента. */

typedef struct
{
  GObject         parent_instance; /* Родительский объект. */
  guint32         start;           /* Индекс первой записи. */
  guint32         end;             /* Индекс последней записи. */
  guint32         n_elements;      /* Количество записей. */
  const gint64   *times;           /* Метки времени. */
  const gdouble  *values;          /* Значения. */
} HyScanNavDataDummy;

typedef struct
{
  GObjectClass parent_class;
} HyScanNavDataDummyClass;

static void hyscan_nav_data_dummy_interface_init (HyScanNavDataInterface *iface);
GType       hyscan_nav_data_dummy_get_type       (void);
G_DEFINE_TYPE_WITH_CODE (HyScanNavDataDummy, hyscan_nav_data_dummy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_NAV_DATA, hyscan_nav_data_dummy_interface_init));

static void
hyscan_nav_data_dummy_class_init (HyScanNavDataDummyClass *klass)
{
}

static void
hyscan_nav_data_dummy_init (HyScanNavDataDummy *dummy)
{
  dummy->times = times;
  dummy->values = values;
  dummy->n_elements = G_N_ELEMENTS (values);
  dummy->start = first_index;
  dummy->end = dummy->start + dummy->n_elements - 1;

}

static gboolean
hyscan_nav_data_dummy_get (HyScanNavData      *ndata,
                           HyScanCancellable  *cancellable,
                           guint32             index,
                           gint64             *time,
                           gdouble            *value)
{
  HyScanNavDataDummy *dummy = HYSCAN_NAV_DATA_DUMMY (ndata);

  if (!(dummy->start <= index && index <= dummy->end))
    return FALSE;

  (time != NULL) ? *time = dummy->times[index - dummy->start] : 0;
  (value != NULL) ? *value = dummy->values[index - dummy->start] : 0;

  return TRUE;
}

static HyScanDBFindStatus
hyscan_nav_data_dummy_find_data (HyScanNavData *ndata,
                                 gint64         time,
                                 guint32       *lindex,
                                 guint32       *rindex,
                                 gint64        *ltime,
                                 gint64        *rtime)
{
  HyScanNavDataDummy *dummy = HYSCAN_NAV_DATA_DUMMY (ndata);
  guint32 i, li = 0, ri = 0;

  if (time < dummy->times[0])
    return HYSCAN_DB_FIND_LESS;
  else if (time > dummy->times[dummy->n_elements - 1])
    return HYSCAN_DB_FIND_GREATER;

  for (i = 0; i <= dummy->n_elements; i++)
    {
      if (dummy->times[i] == time)
        {
          li = ri = i;
          break;
        }
      else if ((i + 1) < dummy->n_elements && dummy->times[i + 1] > time)
        {
          li = i;
          ri = i + 1;
          break;
        }
    }

  (lindex != NULL) ? (*lindex = dummy->start + li) : 0;
  (rindex != NULL) ? (*rindex = dummy->start + ri) : 0;
  (ltime != NULL) ? (*ltime = dummy->times[li]) : 0;
  (rtime != NULL) ? (*rtime = dummy->times[ri]) : 0;

  return HYSCAN_DB_FIND_OK;
}

static gboolean
hyscan_nav_data_dummy_get_range (HyScanNavData *ndata,
                                 guint32       *first,
                                 guint32       *last)
{
  HyScanNavDataDummy *dummy = HYSCAN_NAV_DATA_DUMMY (ndata);

  (first != NULL) ? *first = dummy->start : 0;
  (last != NULL) ? *last = dummy->end : 0;

  return TRUE;
}

static void
hyscan_nav_data_dummy_interface_init (HyScanNavDataInterface *iface)
{
  iface->find_data = hyscan_nav_data_dummy_find_data;
  iface->get = hyscan_nav_data_dummy_get;
  iface->get_range = hyscan_nav_data_dummy_get_range;
}

int
main (int    argc,
      char **argv)
{
  HyScanNavData *nav_data;
  HyScanNavSmooth *smooth, *smooth_circular;
  gdouble value;

  nav_data = g_object_new (HYSCAN_TYPE_NAV_DATA_DUMMY, NULL);

  smooth = hyscan_nav_smooth_new (nav_data);
  smooth_circular = hyscan_nav_smooth_new_circular (nav_data);

  /* Проверям геттер. */
  g_assert_true (hyscan_nav_smooth_get_data (smooth) == nav_data);

  g_assert_true (hyscan_nav_smooth_get (smooth, NULL, 1400, &value));


  /* Пробуем получить значения за пределами доступных данных. */
  g_assert_false (hyscan_nav_smooth_get (smooth, NULL,  500, &value));
  g_assert_false (hyscan_nav_smooth_get (smooth, NULL, 3000, &value));

  /* Значение для момента времени, где известно точное значение. */
  g_assert_cmpfloat (value, ==, 40);

  /* Интерполированные значение между value (1200) = 20 и value (1400) = 30. */
  g_assert_true (hyscan_nav_smooth_get (smooth, NULL, 1300, &value));
  g_assert_cmpfloat (ABS (value - 30), <, 1e-6);
  g_assert_true (hyscan_nav_smooth_get (smooth, NULL, 1260, &value));
  g_assert_cmpfloat (ABS (value - 26), <, 1e-6);

  /* Для не угловых значений среднее между 355 и 5 равно 180. */
  g_assert_true (hyscan_nav_smooth_get (smooth, NULL, 1700, &value));
  g_assert_cmpfloat (ABS (value - 180), <, 1e-2);

  /* Для угловых значений среднее между 355 и 5 равно 0. */
  g_assert_true (hyscan_nav_smooth_get (smooth_circular, NULL, 1700, &value));
  g_assert_cmpfloat (ABS (value - 0), <, 1e-2);
  g_assert_true (hyscan_nav_smooth_get (smooth_circular, NULL, 1750, &value));
  g_assert_cmpfloat (ABS (value - 2.5), <, 1e-2);

  g_object_unref (smooth);
  g_object_unref (smooth_circular);
  g_object_unref (nav_data);

  g_message ("Test done successfully!");

  return 0;
}


#include <hyscan-value-grid.h>

int
main (int    argc,
      char **argv)
{
  HyScanGeoCartesian2D start = { 0, 0 };
  HyScanValueGrid *grid;
  GTimer *timer;
  gint size = 255;
  gdouble step = 1;
  gint n_points = 1000000;

  g_random_set_seed ((guint32) (g_get_monotonic_time () % G_MAXUINT32));

  grid = hyscan_value_grid_new (start, step, size);

  timer = g_timer_new ();
  gint i, j;
  gint total = 0;

  /* Добавляем значения по координатам. */
  // for (i = 0; i < n_points; i++)
  //   {
  //     HyScanGeoCartesian2D point;
  //
  //     point.x = g_random_double_range (-1000, 1000);
  //     point.y = g_random_double_range (-1000, 1000);
  //
  //     if (hyscan_value_grid_add (grid, &point, g_random_double_range (0, 1)))
  //       total++;
  //   }

  /* Добавляем значения по областям. */
  HyScanGeoCartesian2D points[] = {
          { 7., 20. }, { 25., 30. }, { 45., 45. }, { 35., 55. }, { 15., 38. }
          // { 20., 20. }, { 25., 25. }, { 26., 26. }
  };

  for (i = 0; i < 100000; i++)
    hyscan_value_grid_area (grid, points, G_N_ELEMENTS (points), 1.0);

  // gdouble value;
  // for (i = 0; i < size; i++)
  //   {
  //     for (j = 0; j < size; j++)
  //       {
  //         if (hyscan_value_grid_get_index (grid, j, i, &value))
  //           g_print ("%8.2f ", value);
  //         else
  //           g_print ("%8s ", "N/A");
  //       }
  //       g_print ("\n");
  //   }

  g_message ("Points inside: %d (%f%%)", total, 100.0 * total / n_points);
  g_message ("Time elapsed: %f", g_timer_elapsed (timer, NULL));

  g_timer_destroy (timer);
  g_object_unref (grid);

  return 0;
}

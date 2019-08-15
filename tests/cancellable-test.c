#include <hyscan-cancellable.h>
#include <glib/gprintf.h>

#define EQUAL
typedef struct
{
  gfloat current;
  gfloat next;
  gfloat push;
  gfloat pop;
} TestValues;

void
check (gfloat expected,
       gfloat real)
{
  if (ABS (expected - real) > 0.0001)
    {
      g_error ("Expected (%f) and actual (%f) values are not equal. "
               "Did you change internal stack size?",
               expected, real);
    }
}

void
test (HyScanCancellable *c,
      TestValues        *values)
{
  if (values->current == -1)
    goto exit;

  if (c == NULL)
    c = hyscan_cancellable_new ();

  hyscan_cancellable_push (c);
  hyscan_cancellable_set (c, values->current, values->next);
  check (values->push, hyscan_cancellable_get (c));

  /* Спускаемся глубже в кроличью нору. */
  test (g_object_ref (c), values + 1);

  hyscan_cancellable_pop (c);
  check (values->pop, hyscan_cancellable_get (c));

exit:
  g_object_unref (c);
}

TestValues case1[] = {
  {.20, .30, .20, 1.0},
  {.50, .60, .25, .30},
  {.70, 1.00, .257, .26},
  {-1, -1, -1, -1}
};

TestValues case2[] = {
  {0., 1.00, 0., 1.0},
  {0., 1.00, 0., 1.0},
  {.50, .60, .5, 1.0},
  {.50, .60, .55, .6},
  {-1, -1, -1}
};

TestValues case3[] = {
  {0., 1.00, 0., 1.0},
  {1.00, 1.00, 1.00, 1.0}, /* Не надо так делать в продакшене:) */
  {.50, .60, 1.00, 1.0},
  {.50, .60, 1.00, 1.0},
  {-1, -1, -1}
};

TestValues case4[] = {
  {0., 1.0, 0., 1.0},
  {0., 1.0, 0., 1.0},
  {0., 1.0, 0., 1.0},
  {0., 1.0, 0., 1.0},
  {0., 1.0, 0., 1.0},
  {0., 1.0, 0., 1.0},
  {0., 1.0, 0., 1.0}, /* Тут я тестирую чрезмерную вложенность. */
  {0., 1.0, 0.},
  {0., 1.0, 0.},
  {0., 1.0, 0.},
  {0.5, 0.6, 0.},
  {-1, -1, -1}
};

int
main(int argc, char const *argv[])
{
  test (NULL, case1);
  test (NULL, case2);
  test (NULL, case3);
  test (NULL, case4);

  return 0;
}

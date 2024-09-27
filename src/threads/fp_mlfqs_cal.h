#ifndef FP_MLFQS_CAL_H
#define FP_MLFQS_CAL_H

#include <stdint.h>
#define p 17
#define q 14
#define f (1 << q)

inline int n_to_x (int n)
{
  return n * f;
}

inline int x_to_n_zero (int x)
{
  return x / f;
}

inline int x_to_n_near (int x)
{
  return x > 0 ? (x + f / 2) / f : (x - f / 2) / f;
}

inline int add_x_y (int x, int y)
{
  return x + y;
}

inline int sub_x_y (int x, int y)
{
  return x - y;
}

inline int add_x_n (int x, int n)
{
  return x + n * f;
}

inline int sub_x_n (int x, int n)
{
  return x - n * f;
}

inline int mul_x_y (int x, int y)
{
  return ((int64_t) x) * y / f;
}

inline int mul_x_n (int x, int n)
{
  return x * n;
}

inline int div_x_y (int x, int y)
{
  return ((int64_t) x) * f / y;
}

inline int div_x_n (int x, int n)
{
  return x / n;
}

#endif

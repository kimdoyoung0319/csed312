#ifndef FIXED_H
#define FIXED_H

#include <stdint.h>

#define BITS_ABOVE_POINT 17
#define BITS_BELOW_POINT 14
#define FIXED_ONE (1 << BITS_BELOW_POINT)

typedef int32_t fixed;

inline fixed n_to_x (int n)
{
  return n * FIXED_ONE;
}

inline int x_to_n_zero (fixed x)
{
  return x / FIXED_ONE;
}

inline int x_to_n_near (fixed x)
{
  return x > 0 ? (x + FIXED_ONE/2) / FIXED_ONE : (x - FIXED_ONE/2) / FIXED_ONE;
}

inline fixed add_x_y (fixed x, fixed y)
{
  return x + y;
}

inline fixed sub_x_y (fixed x, fixed y)
{
  return x - y;
}

inline fixed add_x_n (fixed x, int n)
{
  return x + n * FIXED_ONE;
}

inline fixed sub_x_n (fixed x, int n)
{
  return x - n * FIXED_ONE;
}

inline fixed mul_x_y (fixed x, fixed y)
{
  return ((int64_t) x) * y / FIXED_ONE;
}

inline fixed mul_x_n (fixed x, int n)
{
  return x * n;
}

inline fixed div_x_y (fixed x, fixed y)
{
  return ((int64_t) x) * FIXED_ONE / y;
}

inline fixed div_x_n (fixed x, int n)
{
  return x / n;
}

#endif

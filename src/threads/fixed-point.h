#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

typedef int fixed_point;

#define FP_DECIMAL 14

#define INT_TO_FP(x) ((fixed_point) ((x) *(1 << FP_DECIMAL)))
#define FP_TO_INT(x) ((x) >= 0 ? ((x) + (1 << (FP_DECIMAL-1))) >> FP_DECIMAL : ((x) - (1 << (FP_DECIMAL-1))) >> FP_DECIMAL)

#define FP_ADD(x , y) ((x) + (y))
#define FP_SUB(x , y) ((x) - (y))
#define FP_MUL(x , y) ((fixed_point)((((int64_t)x) * (y)) >> FP_DECIMAL))
#define FP_DIV(x , y) ((fixed_point)((((int64_t)x) << FP_DECIMAL) / (y)))

#define FP_ADD_INT(x , n) ((x) + (n << FP_DECIMAL))
#define FP_SUB_INT(x , n) ((x) - (n << FP_DECIMAL))
#define FP_MUL_INT(x , n) ((x) * (n))
#define FP_DIV_INT(x , n) ((x) / (n))

#endif /* threads/fixed-point.h */
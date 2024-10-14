#include <stdio.h>

#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

#define INT_TO_FP(A) (A * F)
#define FP_TO_INT_ZERO(A) (A / F)
#define FP_TO_INT_ROUND(A) (((A) >= 0) ? ((A) + F / 2) / F : ((A) - F / 2) / F)
#define ADD_FP(A, B) (A + B)
#define SUB_FP(A, B) (A - B)

#define ADD_MIXED(A, B) (A + B * F)
#define SUB_MIXED(A, B) (A - B * F)
#define MULT_FP(A, B) (((int64_t)A) * B / F)
#define MULT_MIXED(A, B) (A * B)
#define DIV_FP(A, B) (((int64_t)A) * F / B)
#define DIV_MIXED(A, B) (A / B)
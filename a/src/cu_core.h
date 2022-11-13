#if !defined(CU_CORE_H)
/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#define CU_CORE_H
#include <stdint.h>

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))
#define PI 3.14159265
#define CONTAIN(val, minVal, maxVal) max(min(val, maxVal), minVal)
#define EPSILON 1.175494351E-38

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef uint32_t b32;

typedef float f32;
typedef double f64;
#endif
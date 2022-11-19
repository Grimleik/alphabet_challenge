#if !defined(CU_MATH_H)
/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#define CU_MATH_H

#include "cu_core.h"
#include <math.h>

#define VECTOR_TEMPLATE(postfix, type)                                  \
    typedef struct v2##postfix                                          \
    {                                                                   \
        type x;                                                         \
        type y;                                                         \
    } v2##postfix;                                                      \
    inline static v2##postfix v2##postfix##Add(v2##postfix a, v2##postfix b) \
    {                                                                   \
        v2##postfix result = {a.x + b.x, a.y + b.y};                    \
        return result;                                                  \
    }                                                                   \
                                                                        \
    inline static v2##postfix v2##postfix##Sub(v2##postfix a, v2##postfix b) \
    {                                                                   \
        v2##postfix result = {a.x - b.x, a.y - b.y};                    \
        return result;                                                  \
    }                                                                   \
                                                                        \
    inline static v2##postfix v2##postfix##Mul(v2##postfix a, f32 value) \
    {                                                                   \
        v2##postfix result = {(type)(a.x * value), (type)(a.y * value)}; \
        return result;                                                  \
    }                                                                   \
                                                                        \
    inline static v2##postfix v2##postfix##Hadamard(v2##postfix a, v2##postfix b) \
    {                                                                   \
        v2##postfix result = {a.x * b.x, a.y * b.y};                    \
        return result;                                                  \
    }                                                                   \
        

VECTOR_TEMPLATE(f, f32);
VECTOR_TEMPLATE(s, s32);

inline static f32 v2fMagnitude(v2f *src)
{
    return src->x * src->x + src->y * src->y;
}

inline static b32 v2fIsZero(v2f *in)
{
    return !(fabs(in->x) >= EPSILON || fabs(in->y) >= EPSILON);
}

inline static f32 v2fLength(v2f *src)
{
    return (f32)sqrt(src->x * src->x + src->y * src->y);
}

inline static void v2fNormalized(v2f *src)
{
    f32 length = v2fLength(src);
    if (length > EPSILON)
    {
        src->x /= length;
        src->y /= length;
    }
}

inline static void v2fContain(v2f *src, v2f min, v2f max)
{
    src->x = CONTAIN(src->x, min.x, max.x);
    src->y = CONTAIN(src->y, min.y, max.y);
}

inline static v2f v2fNormalize(v2f src)
{
    v2f result = src;
    f32 length = v2fLength(&result);
    if (length > EPSILON)
    {
        result.x /= length;
        result.y /= length;
    }
    return result;
}

static v2f v2fZERO;
static v2f v2fONE = {1, 1};

inline static f32 MapIntoRange01F32(f32 x, f32 min, f32 max)
{
    return (x - min) / (max - min);
}

#endif

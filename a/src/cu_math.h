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
    inline static v2##postfix v2##postfix##Mul(v2##postfix a, type value) \
    {                                                                   \
        v2##postfix result = {a.x * value, a.y * value};                \
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

inline static void v2fNormalize(v2f *src)
{
    f32 length = v2fLength(src);
    if (length > EPSILON)
    {
        src->x /= length;
        src->y /= length;
    }
}


#endif

#if !defined(AGE_H)
#define AGE_H

/* ========================================================================
   $Creator: Patrik Fjellstedt $
   $Sources/Inspirations: Non exhaustive list:
   *Handmade Hero: https://github.com/HandmadeHero
   *olc: https://github.com/OneLoneCoder/olcPixelGameEngine
   $README:
   AGE: Another Graphics/Game Engine is a header only utility file.
   ======================================================================== */
/*
* Rendering:
    * DrawBuffer Calls are packing color, do outside and req. packed in ?
    * DrawBuffer Texture rendering optimization.
* Assets:
    * Asset Format.
        * Asset hot reloading.

* Memory
    * Dynamic Allocator for stb library. Global Arena that can be used for other things aswell ?
    * Partition AND Init for Pools ?
 */

/*==============================INCLUDES==============================*/
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <queue>
/*==============================TYPES==============================*/

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef s32 b32;

typedef size_t MemoryMarker;

/*==============================MACROS==============================*/

#define UNUSED(x) x

#define PI 3.14159265359F
#define RAD2DEG 180.F / PI

#define KB(Value) ((Value)*1024LL)
#define MB(Value) (KB(Value) * 1024LL)
#define GB(Value) (MB(Value) * 1024LL)
#define TB(Value) (GB(Value) * 1024LL)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

#define NOT_YET_IMPLEMENTED(x) error //x

#ifdef _DEBUG
#define Assert(x)       \
    if (!(x))           \
    {                   \
        __debugbreak(); \
    }
#define InvalidCodePath Assert(false)

#else
#define InvalidCodePath
#define Assert(x)

#endif

#ifdef USE_OWN_GLOBAL_ALLOCATOR
struct MemoryArena;
MemoryArena *gMemArena;
void *age_malloc(size_t size);
void *age_realloc(void *p, size_t size);
void *age_free(void *p);
#endif

template <typename T>
inline T Max(T a, T b)
{
    return a > b ? a : b;
}

template <typename T>
inline T Min(T a, T b)
{
    return a < b ? a : b;
}

template <typename T>
inline T Contain(T val, T minVal, T maxVal)
{
    return Max(Min(val, maxVal), minVal);
}

template<typename T>
inline T Wrap(T x, T xMax)
{
    u32 result = (x + xMax) % xMax;
    return result;
}
template<typename T>
inline T Clamp(T x, T xMin, T xMax)
{
    T result = x > xMax ? xMax : (x < xMin ? xMin : x);
    return result;
}

inline f32 SquareF32(f32 value)
{
    f32 result = value * value;
    return result;
}

inline void SetSeed(u32 seed)
{
    srand(seed);
}

inline f32 Random01()
{
    return rand() / (f32)(RAND_MAX);
}

inline f32 RandomF32(f32 min, f32 max)
{
    return min + (Random01() * (max - min));
}

/*==============================DECLARATIONS==============================*/

/*==============================MATH==============================*/
/* TODO:
 * matrix and vec4 SIMD.
 * operators from both sides for scalars.
 * source: https://bitbucket.org/eschnett/vecmathlib/src/master/vec_sse_float4.h
 */

template <typename T>
struct v2
{
    union
    {
        struct
        {
            T x, y;
        };
        struct
        {
            T u, v;
        };
        T e[2];
    };

    inline v2 operator-() { return {-x, -y}; }
    inline v2 perp() { return {-y, x}; }
    inline v2 operator+(const T lh) { return {x + lh, y + lh}; }
    inline v2 operator-(const T lh) { return {x - lh, y - lh}; }
    inline v2 operator+(const v2 &lh) { return {x + lh.x, y + lh.y}; }
    inline v2 operator-(const v2 &lh) { return {x - lh.x, y - lh.y}; }
    inline void operator+=(const T lh)
    {
        x += lh;
        y += lh;
    }
    inline void operator-=(const T lh)
    {
        x -= lh;
        y -= lh;
    }
    inline void operator+=(const v2 &lh)
    {
        x += lh.x;
        y += lh.y;
    }
    inline void operator*=(const T lh)
    {
        x *= lh;
        y *= lh;
    }
    inline void operator-=(const v2 &lh)
    {
        x -= lh.x;
        y -= lh.y;
    }
    inline v2 operator*(const T lh) { return {x * lh, y * lh}; }
    inline v2 operator/(const T lh) { return {x / lh, y / lh}; }
    inline void operator/=(const T lh)
    {
        x / lh;
        y / lh;
    }
    inline v2 hadamard(const v2 &lh) { return {x * lh.x, y * lh.y}; }
    inline T dot(const v2 &lh) { return x * lh.x + y * lh.y; }
    inline T length() { return sqrt(x * x + y * y); }
    inline T length2() { return x * x + y * y; }
    inline v2 normalize() { return *this / length(); }
    inline v2 reflect(v2 n)
    {
        v2 r = *this - 2 * (this->dot(n)) * n;
        return r;
    }
};

template <typename T>
inline v2<T> operator*(const T lh, const v2<T> &rh)
{
    return {lh * rh.x, lh * rh.y};
}

template <typename T>
struct v3
{
    union
    {
        struct
        {
            T x, y, z;
        };
        struct
        {
            T r, g, b;
        };
        T e[3];
    };

    inline v3 operator-() { return {-x, -y, -z}; }
    inline v3 operator+(const T lh) { return {x + lh, y + lh, z + lh}; }
    inline v3 operator-(const T lh) { return {x - lh, y - lh, z - lh}; }
    inline v3 operator+(const v3 &lh) { return {x + lh.x, y + lh.y, z + lh.z}; }
    inline v3 operator-(const v3 &lh) { return {x - lh.x, y - lh.y, z - lh.z}; }
    inline void operator+=(const T lh)
    {
        x += lh;
        y += lh;
        z += lh;
    }
    inline void operator-=(const T lh)
    {
        x -= lh;
        y -= lh;
        z -= lh;
    }
    inline void operator+=(const v3 &lh)
    {
        x += lh.x;
        y += lh.y, z += lh.z;
    }
    inline void operator-=(const v3 &lh)
    {
        x -= lh.x;
        y -= lh.y;
        z -= lh.z;
    }
    inline v3 operator*(const T lh) { return {x * lh, y * lh, z * lh}; }
    inline v3 operator/(const T lh) { return {x / lh, y / lh, z / lh}; }
    inline void operator/=(const T lh)
    {
        x / lh;
        y / lh;
        z / lh;
    }
    inline v3 hadamard(const v3 &lh) { return {x * lh.x, y * lh.y, z * lh.z}; }
    inline v3 cross(const v3 &lh) { return {y * lh.z - z * lh.y, z * lh.x - x * lh.z, x * lh.y - y * lh.x}; }
    inline T dot(const v3 &lh) { return x * lh.x + y * lh.y + z * lh.z; }
    inline T length() { return sqrt(x * x + y * y + z * z); }
    inline T length2() { return x * x + y * y + z * z; }
    inline v3 normalize() { return *this / length(); }
};

template <typename T>
inline v3<T> operator*(const T lh, const v3<T> &rh)
{
    return {lh * rh.x, lh * rh.y, lh * rh.z};
}

template <typename T>
struct v4
{
    union
    {
        struct
        {
            T x, y, z, w;
        };
        struct
        {
            T r, g, b, a;
        };
        T e[4];
    };

    inline v4 operator-() { return {-x, -y, -z, -w}; }
    inline v4 operator+(const T lh) { return {x + lh, y + lh, z + lh, w + lh}; }
    inline v4 operator-(const T lh) { return {x - lh, y - lh, z - lh, w - lh}; }
    inline v4 operator+(const v4 &lh) { return {x + lh.x, y + lh.y, z + lh.z, w + lh.w}; }
    inline v4 operator-(const v4 &lh) { return {x - lh.x, y - lh.y, z - lh.z, w - lh.w}; }
    inline void operator+=(const T lh)
    {
        x += lh;
        y += lh;
        z += lh;
        w += lh;
    }
    inline void operator-=(const T lh)
    {
        x -= lh;
        y -= lh;
        z -= lh;
        w -= lh;
    }
    inline void operator+=(const v4 &lh)
    {
        x += lh.x;
        y += lh.y, z += lh.z, w += lh.w;
    }
    inline void operator-=(const v4 &lh)
    {
        x -= lh.x;
        y -= lh.y;
        z -= lh.z;
        w -= lh.w;
    }
    inline v4 operator*(const T lh) { return {x * lh, y * lh, z * lh, w * lh}; }
    inline v4 operator/(const T lh) { return {x / lh, y / lh, z / lh, w / lh}; }
    inline void operator/=(const T lh)
    {
        x / lh;
        y / lh;
        z / lh;
        w / lh;
    }
    inline v4 hadamard(const v4 &lh) { return {x * lh.x, y * lh.y, z * lh.z, w * lh.w}; }
    inline T dot(const v4 &lh) { return x * lh.x + y * lh.y + z * lh.z + w * lh.w; }
    inline T length() { return sqrt(x * x + y * y + z * z + w * w); }
    inline T length2() { return x * x + y * y + z * z + w * w; }
    inline v4 normalize() { return *this / length(); }
};

template <typename T>
inline v4<T> operator*(const T lh, const v4<T> &rh)
{
    return {lh * rh.x, lh * rh.y, lh * rh.z, lh * rh.w};
}

// NOTE(pf): Step size has to be between 0 and 1.
template <typename T>
inline v4<T> Lerp(const v4<T> &v0, const v4<T> &v1, const f32 stepSize)
{
    return (1.0f - stepSize) * v0 + (stepSize)*v1;
}

typedef v2<f32> v2f;
typedef v2<s32> v2s;
typedef v3<f32> v3f;
typedef v4<s32> v4s;
typedef v4<f32> v4f;

inline u32 ConvertToPackedU32(const v4f &vf)
{
    return ((u32)((u32)(255.0f * vf.a) << 24 |
                  (u32)(255.0f * vf.r) << 16 |
                  (u32)(255.0f * vf.g) << 8 |
                  (u32)(255.0f * vf.b)));
}

inline u32 ConvertToPackedU32(const v4s &vs)
{
    return ((u32)((vs.a) << 24 |
                  (vs.r) << 16 |
                  (vs.g) << 8 |
                  (vs.b)));
}

namespace COLORS
{
    const v4f RED = {{{1.0f, 0.0f, 0.0f, 1.0f}}};
    const v4f GREEN = {0.0f, 1.0f, 0.0f, 1.0f};
    const v4f BLUE = {0.0f, 0.0f, 1.0f, 1.0f};

    const v4f WHITE = {1.0f, 1.0f, 1.0f, 1.0f};
    const v4f BLACK = {0.0f, 0.0f, 0.0f, 1.0f};

    const v4f MAGENTA = {1.0f, 0.0f, 1.0f, 1.0f};
    const v4f ORANGE = {0.85f, 1.0f, 0.0f, 1.0f};
    const v4f YELLOW = {1.0f, 1.0f, 0.0f, 1.0f};

    const v4f GRAY = {0.5f, 0.5f, 0.5f, 1.0f};
    const v4f LIGHTGRAY = {0.75f, 0.75f, 0.75f, 1.0f};
    const v4f DARKGRAY = {0.25f, 0.25f, 0.25f, 1.0f};
}

namespace DIRECTION2D
{
    const v2f UP = {0.0f, 1.0f};
    const v2f RIGHT = {0.0f, 1.0f};
    const v2f DOWN = {0.0f, 1.0f};
    const v2f LEFT = {0.0f, 1.0f};
}

struct Rectangle2
{
    bool Inside(v2f testPos, v2f rectPos)
    {
        v2f halfDim = dim;
        rectPos -= halfDim * 0.5f;
        return (rectPos.x <= testPos.x && testPos.x < (rectPos.x + dim.x) &&
                rectPos.y <= testPos.y && testPos.y < (rectPos.y + dim.y));
    }
    v2f dim;
};

struct Transform
{
    v4f pos;
    v3f scale;
    v3f rot;
};

/*==============================MEMORY==============================*/

/* NOTE(pf):
 * https://github.com/mtrebi/memory-allocators
 */

/* NOTE(pf):
 * Stack -> Implys that you keep track of allocatio/deallocation order. Also it can handle
 * polymorphism with the use of Construct/Deconstruct calls but is mainly intended for
 * block allocations/deallocations with Allocate/Release.
 */
/* TODO(pf):

 * Remove Init version ? Might be a bit too confusing. Test if we can
 * use construct where we would otherwise use init

 */
class MemoryStack
{
public:
    MemoryStack();
    ~MemoryStack();

    void Init(void *memory, MemoryMarker totalSize);
    void *AllocateBytes(MemoryMarker sizeInBytes);
    void *AllocateBytesAndZero(MemoryMarker sizeInBytes);

    // NOTE(pf): DO NOT USE FOR virtual classes!!!
    template <typename T>
    T *Allocate(MemoryMarker count = 1);
    template <typename T>
    T *AllocateAndInit(MemoryMarker count = 1);
    template <typename T>
    T *AllocateAndClone(const T *target);
    template <typename T>
    void Release(MemoryMarker count = 1);
    template <typename T>
    void ReleaseAndZero(MemoryMarker count = 1);

    // NOTE(pf): uses placement new and delete dispatch.
    template <typename T>
    T *AllocateAndConstruct(MemoryMarker count = 1);
    template <typename T>
    void ReleaseAndDeconstruct(MemoryMarker count = 1);

    void Clear();

    MemoryStack *Partition(MemoryMarker size);

    u8 *EndIterator();

    // private:
    void *m_memory;
    MemoryMarker m_totalSize;
    MemoryMarker m_currentSize;
};

/*==============================MEMORY==============================*/

template <typename T>
T *MemoryStack::Allocate(MemoryMarker count)
{
    MemoryMarker sizeInBytes = sizeof(T) * count;
    Assert(m_currentSize + sizeInBytes <= m_totalSize);
    void *result = reinterpret_cast<u8 *>(m_memory) + (m_currentSize);
    m_currentSize += sizeInBytes;
    return (T *)result;
}

template <typename T>
T *MemoryStack::AllocateAndInit(MemoryMarker count)
{
    MemoryMarker sizeInBytes = sizeof(T) * count;
    Assert(m_currentSize + sizeInBytes <= m_totalSize);
    void *pMem = reinterpret_cast<u8 *>(m_memory) + (m_currentSize);
    m_currentSize += sizeInBytes;
    T *result = (T *)pMem;
    for (MemoryMarker i = 0; i < count; ++i)
        *(result + i) = {};
    return result;
}

template <typename T>
T *MemoryStack::AllocateAndClone(const T *target)
{
    MemoryMarker sizeInBytes = sizeof(T);
    Assert(m_currentSize + sizeInBytes <= m_totalSize);
    void *pMem = reinterpret_cast<u8 *>(m_memory) + (m_currentSize);
    m_currentSize += sizeInBytes;
    T *result = (T *)pMem;
    memcpy(result, target, sizeInBytes);
    return result;
}

template <typename T>
void MemoryStack::Release(MemoryMarker marker)
{
    m_currentSize -= marker * sizeof(T);
}

template <typename T>
void MemoryStack::ReleaseAndZero(MemoryMarker marker)
{
    for (u8 *p = ((u8 *)(m_memory) + (m_currentSize - (marker * sizeof(T))));
         p != ((u8 *)(m_memory) + m_currentSize);
         p++)
    {
        *p = 0;
    }
    m_currentSize -= marker * sizeof(T);
}

template <typename T>
T *MemoryStack::AllocateAndConstruct(MemoryMarker count)
{
    MemoryMarker sizeInBytes = sizeof(T) * count;
    Assert(m_currentSize + sizeInBytes <= m_totalSize);
    void *pMem = reinterpret_cast<u8 *>(m_memory) + (m_currentSize);
    T *result = (T *)pMem;
    T *ele;
    for (MemoryMarker i = 0; i < count; ++i)
    {
        pMem = reinterpret_cast<u8 *>(m_memory) + (m_currentSize + (i * sizeof(T)));
        ele = (T *)pMem;
        ele = new (ele) T;
    }
    m_currentSize += sizeInBytes;
    return result;
}

template <typename T>
void MemoryStack::ReleaseAndDeconstruct(MemoryMarker count)
{
    MemoryMarker sizeInBytes = sizeof(T) * count;
    Assert(m_currentSize >= sizeInBytes);

    for (MemoryMarker i = 0; i < count; ++i)
    {
        void *pMem = reinterpret_cast<u8 *>(m_memory) + (m_currentSize - sizeof(T));
        T *ele = (T *)pMem;
        m_currentSize -= sizeof(T);
        ele->~T();
    }
}

template <typename T>
class MemoryPool
{
public:
    struct MemPtr
    {
        T m_element;
        MemPtr *m_next;
    };

    MemoryPool();
    ~MemoryPool();

    void Init();
    void PartitionFrom(MemoryStack *stack, u32 eleCount);

    T *Allocate();
    T *AllocateAndInit();
    T *AllocateAndConstruct();

    void Free(T *ele);
    void FreeAndDeconstruct(T *ele);

    inline u32 ElementsInUse() { return m_elementsInUse; }

private:
    void *m_memory;
    u32 m_totalEleCount;
    u32 m_elementsInUse;

    MemPtr *m_freeLL;
};

template <typename T>
MemoryPool<T>::MemoryPool()
{
    m_memory = nullptr;
    m_totalEleCount = 0;
    m_elementsInUse = 0;

    m_freeLL = nullptr;
}

template <typename T>
MemoryPool<T>::~MemoryPool() {}

template <typename T>
void MemoryPool<T>::Init()
{
    MemPtr *ele = (MemPtr *)m_memory;
    for (u32 ptrIndex = 0; ptrIndex < m_totalEleCount; ++ptrIndex)
    {
        ele->m_next = m_freeLL;
        m_freeLL = ele++;
    }
}

template <typename T>
void MemoryPool<T>::PartitionFrom(MemoryStack *stack, u32 eleCount)
{
    m_memory = stack->AllocateBytes((sizeof(T) + sizeof(MemPtr)) * eleCount);
    m_totalEleCount = eleCount;
    m_elementsInUse = 0;
}

template <typename T>
T *MemoryPool<T>::Allocate()
{
    Assert(m_freeLL != nullptr);
    T *result = &m_freeLL->m_element;
    m_freeLL = m_freeLL->m_next;
    ++m_elementsInUse;
    return result;
}

template <typename T>
T *MemoryPool<T>::AllocateAndInit()
{
    Assert(m_freeLL != nullptr);
    T *result = &m_freeLL->m_element;
    m_freeLL = m_freeLL->m_next;
    ++m_elementsInUse;
    *result = {};
    return result;
}

template <typename T>
T *MemoryPool<T>::AllocateAndConstruct()
{
    Assert(m_freeLL != nullptr);
    T *result = &m_freeLL->m_element;
    m_freeLL = m_freeLL->m_next;
    ++m_elementsInUse;
    result = new (result) T;
    return result;
}

template <typename T>
void MemoryPool<T>::Free(T *ele)
{
    MemPtr *push = (MemPtr *)ele;
    push->m_next = m_freeLL;
    m_freeLL = push;
    --m_elementsInUse;
}

template <typename T>
void MemoryPool<T>::FreeAndDeconstruct(T *ele)
{
    MemPtr *push = (MemPtr *)ele;
    push->m_next = m_freeLL;
    m_freeLL = push;
    --m_elementsInUse;
    ele->~T();
}

/*==============================RENDERING==============================*/

struct TextureInfo
{
    u8 *memory;
    s32 width;
    s32 height;
    s32 bytesPerPixel;
};

TextureInfo LoadTexture(char *imageFile);
void FreeImage(TextureInfo *info);

struct stbtt_fontinfo;
struct TextInfo
{
    char *txt;
    stbtt_fontinfo *font;
    f32 scale = 1.0f;
    b32 centered;
};

stbtt_fontinfo *LoadFont(char *fontFile, MemoryStack *allocator);

struct DrawBuffer
{
    void Clear(v4f color);
    void DrawQuad(s32 x, s32 y, s32 width, s32 height, v4f color);
    void DrawQuadOutline(s32 x, s32 y, s32 width, s32 height, s32 thickness, v4f color);
    void DrawLine(s32 x0, s32 y0, s32 x1, s32 y1, v4f color0, v4f color1);
    void DrawPixel(s32 x, s32 y, v4f color);
    void DrawTextCmd(s32 x, s32 y, v4f color, TextInfo *info);
    void DrawTexture(s32 x, s32 y, v4f tint, TextureInfo *info);

    void *m_memory;
    s32 m_width;
    s32 m_height;
    u32 m_pitch;
    u32 m_bytesPerPixel;
};

enum class RC_TYPE
{
    RC_ClearColor,
    RC_DrawQuad,
    RC_DrawQuadOutline,
    RC_DrawLine,
    RC_DrawCircle,
    RC_DrawCircleOultine,
    RC_DrawText,
    RC_DrawTexture,
};

struct RenderCommand
{
    RenderCommand(){};
    ~RenderCommand(){};

    RC_TYPE type;
};

struct RCClearColor : public RenderCommand
{
    RCClearColor()
    {
        type = RC_TYPE::RC_ClearColor;
    };
    ~RCClearColor(){};

    v4f color;
};

struct RCDrawQuad : public RenderCommand
{
    RCDrawQuad()
    {
        type = RC_TYPE::RC_DrawQuad;
    };
    ~RCDrawQuad(){};

    v4f color;
    v2f pos;
    v2f dim;
};

struct RCDrawLine : public RenderCommand
{
    RCDrawLine()
    {
        type = RC_TYPE::RC_DrawLine;
    };
    ~RCDrawLine(){};

    v4f colorStart;
    v4f colorEnd;
    v2f start;
    v2f end;
};

struct RCDrawCircle : public RenderCommand
{
    RCDrawCircle()
    {
        type = RC_TYPE::RC_DrawCircle;
    };
    v2f center;
    f32 radius;
    v4f color;
};

struct RCDrawCircleOutline : public RenderCommand
{
    RCDrawCircleOutline()
    {
        type = RC_TYPE::RC_DrawCircleOultine;
    };
    v2f center;
    f32 radius;
    f32 thickness;
    v4f color;
};

struct RCDrawQuadOutline : public RenderCommand
{
    RCDrawQuadOutline()
    {
        type = RC_TYPE::RC_DrawQuadOutline;
    };
    ~RCDrawQuadOutline(){};

    v4f color;
    v2f pos;
    v2f dim;
    f32 thickness;
};

struct RCDrawText : public RenderCommand
{
    RCDrawText()
    {
        type = RC_TYPE::RC_DrawText;
    };
    ~RCDrawText(){};

    TextInfo txtInfo;
    v4f color;
    v2f pos;
};

struct RCDrawTexture : public RenderCommand
{
    RCDrawTexture()
    {
        type = RC_TYPE::RC_DrawTexture;
    }

    TextureInfo *info;
    v2f pos;
    v4f color_tint;
};

enum class RENDERGROUP_TYPE
{
    ORTHOGRAPHIC,
    UI,
};

struct RenderGroup
{
    void Init(MemoryStack *partition);

    template <typename T = RenderCommand>
    inline T *PushCommand()
    {
        return m_commandStack->AllocateAndInit<T>();
    }

    RENDERGROUP_TYPE m_type;
    f32 m_metersToPixels;
    f32 m_widthOverHeight;
    v2f m_offset;
    MemoryStack *m_commandStack;
};

struct RenderSettings
{
    b32 isVsync;
    b32 shouldToggleFullscreen;
};

struct RenderList
{
    RenderGroup *PushRenderGroup();
    u32 windowWidth;
    u32 windowHeight;
    f32 aspectRatio;
    f32 metersToPixels;
    MemoryStack renderGroups;
    RenderSettings *settings;
};

v2s GetTextDimensions(TextInfo *info);

/*=================================INPUT================================*/
enum FRAME
{
    CURRENT = 0,
    PREVIOUS = 1,
    _COUNT,
};

enum AXIS
{
    MOUSE,
    MOUSE_WHEEL,
    GP_LEFT,
    GP_RIGHT,
    MAX_AXIS_STATES,
};

// NOTE(pf): These are based on windows virtual keys, other platforms might have
// to do a mapping function in the input handling.
enum KEY
{
    M1 = 1,
    M2 = 2,
    ARROW_LEFT = 0x25,
    ARROW_UP = 0x26,
    ARROW_RIGHT = 0x27,
    ARROW_DOWN = 0x28,
    ESCAPE = 0x1B,
    SPACE = 0x20,
    A = 'A',
    B = 'B',
    C = 'C',
    D = 'D',
    E = 'E',
    F = 'F',
    G = 'G',
    H = 'H',
    I = 'I',
    J = 'J',
    K = 'K',
    L = 'L',
    M = 'M',
    N = 'N',
    O = 'O',
    P = 'P',
    Q = 'Q',
    R = 'R',
    S = 'S',
    T = 'T',
    U = 'U',
    V = 'V',
    W = 'W',
    X = 'X',
    Y = 'Y',
    Z = 'Z',
    a = 'a',
    b = 'b',
    c = 'c',
    d = 'd',
    e = 'e',
    f = 'f',
    g = 'g',
    h = 'h',
    i = 'i',
    j = 'j',
    k = 'k',
    l = 'l',
    m = 'm',
    n = 'n',
    o = 'o',
    p = 'p',
    q = 'q',
    r = 'r',
    s = 's',
    t = 't',
    u = 'u',
    v = 'v',
    w = 'w',
    x = 'x',
    y = 'y',
    z = 'z',
    K0 = '0',
    K1 = '1',
    K2 = '2',
    K3 = '3',
    K4 = '4',
    K5 = '5',
    K6 = '6',
    K7 = '7',
    K8 = '8',
    K9 = '9',
    MAX_KEY_STATES = 0xFF
};

struct Input
{
    struct KeyState
    {
        b32 isDown = false;
    };

    struct AxisState
    {
        f32 x, y;
    };

    void Update();
    void UpdateKey(u32 keyCode, b32 keyState);
    void UpdateAxis(u8 axisCode, f32 x, f32 y);

    b32 KeyPressed(u32 key, FRAME f = FRAME::CURRENT);
    b32 KeyDown(u32 key, FRAME f = FRAME::CURRENT);
    b32 KeyReleased(u32 key, FRAME f = FRAME::CURRENT);

    f32 AxisX(u32 axis, FRAME f = FRAME::CURRENT);
    f32 AxisY(u32 axis, FRAME f = FRAME::CURRENT);
    v2f Axis(u32 axis, FRAME f = FRAME::CURRENT);

    KeyState m_keyStates[FRAME::_COUNT][MAX_KEY_STATES];
    AxisState m_axisStates[FRAME::_COUNT][MAX_AXIS_STATES];
    u8 m_activeFrame = 0;
};

/*==============================PLATFORM_INTERFACE==============================*/

struct PlatformState;
#define GAME_INIT(name) void name(PlatformState *platformState, b32 reloaded)
typedef GAME_INIT(GameInit_t);

#define GAME_UPDATE(name) void name(PlatformState *platformState)
typedef GAME_UPDATE(GameUpdate_t);

#define GAME_SHUTDOWN(name) void name(PlatformState *platformState)
typedef GAME_SHUTDOWN(GameShutdown_t);

#define HI_RES_PERF_FREQ(name) s64 name()
typedef HI_RES_PERF_FREQ(HiResPerformanceFreq_t);

#define HI_RES_PERF_QUERY(name) s64 name()
typedef HI_RES_PERF_QUERY(HiResPerformanceQuery_t);

struct PlatformState
{
    HiResPerformanceQuery_t *PerfQuery;

    Input *input;
    RenderList *renderList;
    void *memory;
    MemoryMarker memorySize;

    s64 performanceFreq;
    f32 deltaTime;
    b32 isRunning;

    // NOTE(pf): Pointer to platform specifics that the platform allocates and
    // can use. NEVER USE THIS OUTSIDE THE PLATFORM!!!
    void *platformHandle;
};

#endif

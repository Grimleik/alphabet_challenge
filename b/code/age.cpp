/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#include "age.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../externals/stb_truetype.h"
#ifdef USE_OWN_GLOBAL_ALLOCATOR
#error NOT YET IMPLEMENTED!
// TODO: Own global allocators
void *age_malloc(size_t size)
{
    NOT_YET_IMPLEMENTED;
}
void *age_realloc(void *p, size_t newsz)
{
    NOT_YET_IMPLEMENTED;
}
void *age_free(void *p)
{
    NOT_YET_IMPLEMENTED;
}
#define STB_MALLOC(sz) age_malloc(sz)
#define STB_REALLOC(p, newsz) age_realloc(p, newsz)
#define STB_FREE(p) age_free(p)
#endif
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(disable : 4244)
#include "../externals/stb_image.h"
#pragma warning(default : 4244)

MemoryStack::MemoryStack()
{
    m_memory = nullptr;
    m_totalSize = 0;
    m_currentSize = 0;
}

MemoryStack::~MemoryStack() {}

void MemoryStack::Init(void *mem, MemoryMarker totSize)
{
    m_memory = mem;
    m_totalSize = totSize;
    m_currentSize = 0;
}

void *MemoryStack::AllocateBytes(MemoryMarker sizeInBytes)
{
    Assert(m_currentSize + sizeInBytes <= m_totalSize);
    void *result = reinterpret_cast<u8 *>(m_memory) + (m_currentSize);
    m_currentSize += sizeInBytes;
    return result;
}

void *MemoryStack::AllocateBytesAndZero(MemoryMarker sizeInBytes)
{
    void *result = AllocateBytes(sizeInBytes);
    memset(result, 0, sizeInBytes);
    return result;
}

void MemoryStack::Clear()
{
    m_currentSize = 0;
}

MemoryStack *MemoryStack::Partition(MemoryMarker size)
{
    MemoryStack *result = AllocateAndInit<MemoryStack>();
    result->m_memory = AllocateBytes(size);
    result->m_totalSize = size;
    return result;
}

u8 *MemoryStack::EndIterator()
{
    return (u8 *)m_memory + m_currentSize;
}

/*==============================RENDERING==============================*/

void RenderGroup::Init(MemoryStack *partition)
{
    m_commandStack = partition;
}

void DrawBuffer::Clear(v4f color)
{
    u32 packedColor = ConvertToPackedU32(color);

    // NOTE(pf): Dont care about alpha for this opt.
    if ((packedColor << 8) == 0)
    {
        memset(m_memory, 0, m_width * m_height * m_bytesPerPixel);
    }
    else
    {
        // TODO(pf): First thing to go wide.
        u32 *pixel = (u32 *)m_memory;
        // NOTE(pf): ARGB
        for (int i = 0; i < (m_width * m_height); ++i)
        {
            *pixel++ = packedColor;
        }
    }
}

void DrawBuffer::DrawQuad(s32 x, s32 y, s32 width, s32 height, v4f color)
{
    width = (u32)(width * 0.5f);
    height = (u32)(height * 0.5f);
    s32 xMin = Max<s32>(x - width, 0);
    s32 xMax = Max<s32>(Min<s32>(x + width, m_width), 0);
    s32 yMin = Max<s32>(y - height, 0);
    s32 yMax = Max<s32>(Min<s32>(y + height, m_height), 0);
    u32 packedColor = ConvertToPackedU32(color);

    for (s32 j = yMin; j < yMax; ++j)
    {
        u32 *pixel = (u32 *)(m_memory) + j * m_width;
        for (s32 i = xMin; i < xMax; ++i)
        {
            *(pixel + i) = packedColor;
        }
    }
}

void DrawBuffer::DrawQuadOutline(s32 x, s32 y, s32 width, s32 height, s32 thickness, v4f color)
{
    width = (u32)(width * 0.5f);
    height = (u32)(height * 0.5f);
    s32 xMin = Max<s32>(x - width, 0);
    s32 xMax = Max<s32>(Min<s32>(x + width, m_width), 0);
    s32 yMin = Max<s32>(y - height, 0);
    s32 yMax = Max<s32>(Min<s32>(y + height, m_height), 0);
    s32 halfThickness = (s32)(thickness * 0.5f);
    u32 packedColor = ConvertToPackedU32(color);

    // UPPER:
    for (s32 j = Max<s32>(yMax - halfThickness, 0); j < Min<s32>(yMax + halfThickness, m_height); ++j)
    {
        u32 *pixel = (u32 *)(m_memory) + j * m_width;
        for (s32 i = xMin; i < xMax; ++i)
        {
            *(pixel + i) = packedColor;
        }
    }

    // RIGHT:
    for (s32 j = yMin; j < yMax; ++j)
    {
        u32 *pixel = (u32 *)(m_memory) + j * m_width;
        for (s32 i = Max<s32>(xMax - halfThickness, 0); i < Min<s32>(xMax + halfThickness, m_width); ++i)
        {
            *(pixel + i) = packedColor;
        }
    }

    // LOWER:
    for (s32 j = Max<s32>(yMin - halfThickness, 0); j < Min<s32>(yMin + halfThickness, m_height); ++j)
    {
        u32 *pixel = (u32 *)(m_memory) + j * m_width;
        for (s32 i = xMin; i < xMax; ++i)
        {
            *(pixel + i) = packedColor;
        }
    }

    // LEFT:
    for (s32 j = yMin; j < yMax; ++j)
    {
        u32 *pixel = (u32 *)(m_memory) + j * m_width;
        for (s32 i = Max<s32>(xMin - halfThickness, 0); i < Min<s32>(xMin + halfThickness, m_width); ++i)
        {
            *(pixel + i) = packedColor;
        }
    }
}

void DrawBuffer::DrawLine(s32 x0, s32 y0, s32 x1, s32 y1,
                          v4f color0, v4f color1)
{
    x0 = Max<s32>(Min<s32>(x0, m_width), 0);
    x1 = Max<s32>(Min<s32>(x1, m_width), 0);
    y0 = Max<s32>(Min<s32>(y0, m_height), 0);
    y1 = Max<s32>(Min<s32>(y1, m_height), 0);

    s32 xMin = Max<s32>(Min<s32>(x0, x1), 0);
    s32 xMax = Min<s32>(Max<s32>(x0, x1), m_width);

    s32 yMin = Max<s32>(Min<s32>(y0, y1), 0);
    s32 yMax = Min<s32>(Max<s32>(y0, y1), m_height);

    s32 steps = Max<s32>((yMax - yMin), (xMax - xMin));
    f32 dx = (x1 - x0) / (f32)steps;
    f32 dy = (y1 - y0) / (f32)steps;

    f32 stepSize = 1.0f / steps;
    for (s32 step = 0; step < steps; ++step)
    {
        s32 x = Max<s32>(Min<s32>((s32)(x0 + (dx * step)), m_width), 0);
        s32 y = Max<s32>(Min<s32>((s32)(y0 + (dy * step)), m_height), 0);

        u32 *pixel = (u32 *)(m_memory) + ((y * m_width) + x);
        v4f lerpC = Lerp(color0, color1, stepSize * step);
        u32 packedColor = ConvertToPackedU32(lerpC);
        *(pixel) = packedColor;
    }
}

void DrawBuffer::DrawPixel(s32 x, s32 y, v4f color)
{
    u32 *pixel = (u32 *)(m_memory) + ((y * m_width) + x);

    *(pixel) = (((u8)(color.r * 255)) << 16 |
                ((u8)(color.g * 255)) << 8 |
                ((u8)(color.b * 255)) << 0 |
                ((u8)(color.a * 255)) << 24);
}

void DrawBuffer::DrawTextCmd(s32 x, s32 y, v4f color, TextInfo *info)
{
    f32 pixelScale = stbtt_ScaleForPixelHeight(info->font, info->scale);
    char *text = info->txt;
    u32 packedColor = ConvertToPackedU32(color);
    if (info->centered)
    {
        v2s dims = GetTextDimensions(info);
        x -= (s32)(dims.x * 0.5f);
        y -= (s32)(dims.y * 0.5f);
    }
    while (*text)
    {
        int advance, offsetX, offsetY;
        char c = *text;
        int w, h, i, j;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(info->font, 0,
                                                         pixelScale, c, &w, &h, &offsetX, &offsetY);
        u32 *drawPtr = ((u32 *)m_memory); // + (drawY * m_width) + drawX;
        for (j = 0; j < h; ++j)
        {
            for (i = 0; (i < w); ++i)
            {
                if (bitmap[j * w + i])
                {
                    int drawX = (i + offsetX + x);
                    int drawY = (j + offsetY + y);
                    if (drawX > 0 && drawX < m_width &&
                        drawY > 0 && drawY < m_height)
                    {
                        *(drawPtr + drawX + drawY * m_width) = packedColor;
                    }
                }
            }
        }

        // .. kerning relative to other characters in the text.
        stbtt_GetCodepointHMetrics(info->font, c, &advance, 0);
        x += (s32)(advance * pixelScale);
        if (c)
        {
            x += (s32)(pixelScale * stbtt_GetCodepointKernAdvance(info->font, c, *(text + 1)));
        }

        if (x >= m_width)
            break;

        ++text;
    }
}

void DrawBuffer::DrawTexture(s32 x, s32 y, v4f tint, TextureInfo *info)
{
    s32 width = (u32)(info->width * 0.5f);
    s32 height = (u32)(info->height * 0.5f);
    s32 xMin = Max<s32>(x - width, 0);
    s32 xMax = Max<s32>(Min<s32>(x + width, m_width), 0);
    s32 yMin = Max<s32>(y - height, 0);
    s32 yMax = Max<s32>(Min<s32>(y + height, m_height), 0);
    u32 tintColor = ConvertToPackedU32(tint);

    for (s32 j = yMin; j < yMax; ++j)
    {
        u32 *pixel = (u32 *)(m_memory) + j * m_width;
        u32 *texture = (u32 *)(info->memory) + (j - yMin) * info->width;
        for (s32 i = xMin; i < xMax; ++i)
        {
            *(pixel + i) = *(texture + (i - xMin)) + tintColor;
        }
    }
}

RenderGroup *RenderList::PushRenderGroup()
{
    RenderGroup *result = renderGroups.AllocateAndInit<RenderGroup>();
    result->m_metersToPixels = metersToPixels;
    result->m_widthOverHeight = aspectRatio;
    result->m_commandStack = renderGroups.Partition(KB(10));

    return result;
}

/*==============================INPUT==============================*/

void Input::Update()
{
    m_activeFrame = (m_activeFrame + 1) % FRAME::_COUNT;
    memcpy(m_keyStates[m_activeFrame],
           m_keyStates[Wrap<u32>(m_activeFrame - 1, FRAME::_COUNT)],
           MAX_KEY_STATES * sizeof(KeyState));
}

void Input::UpdateKey(u32 keyCode, b32 keyState)
{
    m_keyStates[m_activeFrame][keyCode].isDown = keyState;
}

void Input::UpdateAxis(u8 axisCode, f32 x, f32 y)
{
    m_axisStates[m_activeFrame][axisCode].x = x;
    m_axisStates[m_activeFrame][axisCode].y = y;
}

b32 Input::KeyPressed(u32 key, FRAME f)
{
    return (m_keyStates[Wrap<u32>(m_activeFrame - f, FRAME::_COUNT)][key].isDown &&
            !m_keyStates[Wrap<u32>(m_activeFrame - f - FRAME::PREVIOUS, FRAME::_COUNT)][key].isDown);
}

b32 Input::KeyDown(u32 key, FRAME f)
{
    return m_keyStates[Wrap<u32>(m_activeFrame - f, FRAME::_COUNT)][key].isDown;
}

b32 Input::KeyReleased(u32 key, FRAME f)
{
    return (!m_keyStates[Wrap<u32>(m_activeFrame - f, FRAME::_COUNT)][key].isDown &&
            m_keyStates[Wrap<u32>(m_activeFrame - f - FRAME::PREVIOUS, FRAME::_COUNT)][key].isDown);
}

f32 Input::AxisX(u32 axis, FRAME f)
{
    return m_axisStates[f][axis].x;
}

f32 Input::AxisY(u32 axis, FRAME f)
{
    return m_axisStates[f][axis].y;
}

v2f Input::Axis(u32 axis, FRAME f)
{
    return v2f{m_axisStates[f][axis].x, m_axisStates[f][axis].y};
}

TextureInfo LoadTexture(const char *imageFile)
{
    TextureInfo result;
    result.memory = stbi_load(imageFile, &result.width, &result.height, &result.bytesPerPixel, 4);
    return result;
}

void FreeImage(TextureInfo *info)
{
    stbi_image_free(info->memory);
}

stbtt_fontinfo *LoadFont(char *fontFile, MemoryStack *allocator)
{
    stbtt_fontinfo *result = allocator->Allocate<stbtt_fontinfo>();
    unsigned char *ttf_buffer = (unsigned char *)allocator->AllocateBytes(sizeof(unsigned char) * 1 << 25);
    fread(ttf_buffer, 1, 1 << 25, fopen(fontFile, "rb"));
    stbtt_InitFont(result, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
    return result;
}

v2s GetTextDimensions(TextInfo *info)
{
    v2s result;
    result.x = 0;
    result.y = 0;
    f32 pixelScale = stbtt_ScaleForPixelHeight(info->font, info->scale);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(info->font, &ascent, &descent, &lineGap);
    result.y = ascent + descent + lineGap;
    char *text = info->txt;
    while (*text)
    {
        int advance;
        char c = *text;
        int w, h;
        stbtt_GetCodepointBitmap(info->font, 0,
                                 pixelScale, c, &w, &h, 0, 0);

        // .. kerning relative to other characters in the text.
        stbtt_GetCodepointHMetrics(info->font, c, &advance, 0);
        result.x += advance;
        if (c)
        {
            result.x += (s32)(stbtt_GetCodepointKernAdvance(info->font, c, *(text + 1)));
        }
        ++text;
    }

    result = v2s{(s32)(result.x * pixelScale), (s32)(result.y * pixelScale)};
    return result;
}

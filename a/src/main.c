#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "cu_core.h"
#include "cu_math.h"
#include <time.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../shared/stb_truetype.h"

/* Potential TODOS
 * AlienZ.
 * Powerups.
 * Performance Checks, we have some bad code atm (rendering spec.).
 * Transition from Programatic Art to 'Prettier' Programatic Art ?
 */

#define ASSERT(x)     \
    if (!(x))         \
    {                 \
        DebugBreak(); \
        int a = 5;    \
    }

#define INVALID_DEFAULT_CASE \
    default:                 \
    {                        \
        ASSERT(0);           \
    }
#undef RGB
#define RGB(r, g, b) (u32)((r) << 16 | (g) << 8 | (b))
enum KEY_STATE
{
    KS_UP,
    KS_DOWN,
};

enum GAME_STATE
{
    GS_MENU,
    GS_GAME_SETUP,
    GS_GAME,
    GS_PAUSED,
    GS_GAME_OVER,
};

enum COLLISION_TYPE
{
    BULLET = 0x1,
    PLAYER = 0x2,
    ASTEROID = 0x4,
    ALIEN = 0x8,
};

typedef struct KeyState
{
    u8 prevState;
    u8 currState;
} KeyState;

typedef struct RenderBuffer
{
    s32 width;
    s32 height;
    void *memory;
    u32 memorySize;
    BITMAPINFO bitsInfo;
} RenderBuffer;

typedef enum AsteroidStage
{
    AS_BIG,
    AS_SMALL
} AsteroidStage;

typedef struct Entity
{
    // SHARED:
    v2f pos;
    v2f dP;
    v2f dir;
    v2f halfDim;
    u32 color;

    u32 collisionMask;
    u32 collisionType;

    // SPECIFICS:
    u32 health;
    f32 maxVel;
    union
    {
        f32 lifeTime_u;
        f32 invulnearableTimer_u;
    };
    u32 stage;
    struct Entity *next;
    struct Entity *prev;
} Entity;

typedef struct GameState
{
    u32 asteroidCount;
    u32 asteroidMaxCount;
    f32 frameDt;
    f32 asteroidSpawnTimer;
    Entity *freeList;
    Entity *activeEntityList;
    Entity *entityList;
    u32 entityCount;
    Entity *player;
    RenderBuffer *rb;
    v2s screenDim;
    HWND hwnd;
    u8 mode;
    u32 score;

    f32 shootingDelay;
    f32 maxShootingDelay;

    stbtt_fontinfo font;

} GameState;

// GLOBALS:
struct KeyState keyStates[0xFE];
#define ENTITY_COUNT 256

// 0, 1
inline f32 RandomNormalized01()
{
    return (f32)(rand()) / RAND_MAX;
}

// -1 . 1
inline f32 RandomNormalized()
{
    f32 result = RandomNormalized01();
    return (result * 2) - 1;
}

#define RANDOM_BETWEEN(name, type)                         \
    inline type Random##name##Between(type min, type max)  \
    {                                                      \
        return (RandomNormalized01() * (max - min)) + min; \
    }

// NOTE(pf): CBA To create a bunch of math util functions atm, just
// here to test the possibilities of macros to create generic
// functions for the future.
RANDOM_BETWEEN(F32, f32)
// RANDOM_BETWEEN(V2f, v2f) NOTE: No operator overloading ):

inline u32 RandomColorBetween(u8 minR, u8 minG, u8 minB,
                              u8 maxR, u8 maxG, u8 maxB)
{
    u8 r = (u8)(RandomNormalized01() * (maxR - minR)) + minR;
    u8 g = (u8)(RandomNormalized01() * (maxG - minG)) + minG;
    u8 b = (u8)(RandomNormalized01() * (maxB - minB)) + minB;
    u32 result = r << 16 | g << 8 | b;
    return result;
}

inline v2f RandomV2fBetween(v2f minP, v2f maxP)
{
    v2f result;
    result.x = RandomNormalized01() * (maxP.x - minP.x) + minP.x;
    result.y = RandomNormalized01() * (maxP.y - minP.y) + minP.y;
    return result;
}

static Entity *EntityCreate(GameState *gameState, u32 color, v2f pos, v2f dim,
                            u32 collisionMask, u32 collisionType, f32 maxVel)
{
    Entity *result;
    if (gameState->freeList)
    {
        result = gameState->freeList;
        gameState->freeList = gameState->freeList->next;
        if (gameState->freeList)
            gameState->freeList->prev = result->prev;
    }
    else
    {
        ASSERT(gameState->entityCount < ENTITY_COUNT);
        result = gameState->entityList + gameState->entityCount++;
    }

    result->color = color;
    result->pos = pos;
    result->dP.x = 0.0f;
    result->dP.y = 0.0f;
    result->halfDim = dim;
    result->collisionMask = collisionMask;
    result->collisionType = collisionType;
    result->health = collisionType == PLAYER ? 3 : 1;
    result->prev = result->next = 0;
    result->dir.x = 1.0f;
    result->dir.y = 0.0f;
    result->maxVel = maxVel;
    result->stage = 0;
    if (collisionType == BULLET)
    {
        result->lifeTime_u = 1.0f;
    }

    if (gameState->activeEntityList)
    {
        // result->prev = gameState->activeEntityList->prev;
        result->next = gameState->activeEntityList;
        gameState->activeEntityList->prev = result;
    }

    gameState->activeEntityList = result;
    return result;
}

static Entity *AsteroidCreate(GameState *gameState, v2f pos, v2f halfDim, AsteroidStage stage)
{
    Entity *result = EntityCreate(gameState, RGB(40, 40, 40), pos, halfDim, BULLET, ASTEROID, 5.0f);
    v2f minVel = {-2.f, -2.f};
    v2f maxVel = {2.0f, 2.0f};
    result->dP = RandomV2fBetween(minVel, maxVel);
    result->stage = stage;
    gameState->asteroidCount++;
    return result;
}

static Entity *PlayerCreate(GameState *gameState, v2f pos)
{
    const v2f playerDim = {15, 15};
    Entity *result = EntityCreate(gameState, RGB(255, 255, 255), pos, playerDim, ASTEROID | ALIEN, PLAYER, 8.f);
    return result;
}

// TODO: This needs another pass.
static void EntityFree(Entity *entity, GameState *gameState)
{
    if (gameState->activeEntityList == entity)
    {
        gameState->activeEntityList = entity->next;
    }
    else
    {
        ASSERT(entity->next || entity->prev);
        if (entity->next)
            entity->next->prev = entity->prev;
        if (entity->prev)
            entity->prev->next = entity->next;
    }

    if (gameState->freeList)
    {
        entity->next = gameState->freeList->next;
        entity->prev = gameState->freeList->prev;
        gameState->freeList = entity;
    }
    else
    {
        gameState->freeList = entity;
        entity->prev = entity->next = 0;
    }
}

inline b32 EntityCollidesWith(Entity *a, Entity *b)
{
    b32 result = 0;
    if (a->collisionMask & b->collisionType)
    {
        v2f pos = v2fSub(b->pos, a->pos);
        v2f minkowskiDim;
        minkowskiDim.x = (a->halfDim.x + b->halfDim.x);
        minkowskiDim.y = (a->halfDim.y + b->halfDim.y);

        if (pos.x >= -minkowskiDim.x && pos.x < minkowskiDim.x &&
            pos.y >= -minkowskiDim.y && pos.y < minkowskiDim.y)
        {
            result = 1;
        }
    }

    return result;
}

static void EntityWrapToWorld(Entity *entity, s32 minX, s32 minY, s32 maxX, s32 maxY)
{
    if (entity->pos.x < minX)
    {
        entity->pos.x = maxX - entity->pos.x;
    }
    else if (entity->pos.x > maxX)
    {
        entity->pos.x = entity->pos.x - maxX;
    }

    if (entity->pos.y < minY)
    {
        entity->pos.y = maxY - entity->pos.y;
    }
    else if (entity->pos.y > maxY)
    {
        entity->pos.y = entity->pos.y - maxY;
    }
}

static Entity *EntityCollidesWithAnything(Entity *entity, Entity *entityList)
{
    Entity *result = 0;
    for (Entity *collider = entityList;
         collider;
         collider = collider->next)
    {
        if (entity != collider && EntityCollidesWith(entity, collider))
        {
            result = collider;
            break;
        }
    }

    return result;
}

static void RenderBufferClear(RenderBuffer *rb)
{
    memset(rb->memory, 0, rb->memorySize);
}

static void RenderBufferDrawCircle(RenderBuffer *rb, s32 x, s32 y, s32 radius, u32 color)
{
    s32 drawX = CONTAIN(x - radius, 0, rb->width);
    s32 drawY = CONTAIN(y - radius, 0, rb->height);
    u32 drawWidth = CONTAIN(x + radius, 0, rb->width);
    u32 drawHeight = CONTAIN(y + radius, 0, rb->height);
    for (s32 itY = drawY; itY < (s32)drawHeight; ++itY)
    {
        for (s32 itX = drawX; itX < (s32)drawWidth; ++itX)
        {
            v2f p, pNorm;
            p.x = (f32)(itX - x) / radius;
            p.y = (f32)(itY - y) / radius;
            pNorm = v2fNormalize(p);
            if (v2fLength(&p) <= v2fLength(&pNorm))
            {
                u32 *pixel = (u32 *)rb->memory + ((rb->width * itY) + itX);
                *pixel = color;
            }
        }
    }
}

static void RenderBufferDrawQuad(RenderBuffer *rb, s32 x, s32 y, s32 halfWidth, s32 halfHeight, u32 color)
{
    s32 drawX = CONTAIN(x - halfWidth, 0, rb->width);
    s32 drawY = CONTAIN(y - halfHeight, 0, rb->height);
    u32 drawWidth = CONTAIN(x + halfWidth, 0, rb->width);
    u32 drawHeight = CONTAIN(y + halfHeight, 0, rb->height);
    for (u32 itY = drawY; itY < drawHeight; ++itY)
    {
        for (u32 itX = drawX; itX < drawWidth; ++itX)
        {
            u32 *pixel = (u32 *)rb->memory + ((rb->width * itY) + itX);
            *pixel = color;
        }
    }
}

static void RenderBufferDrawQuadOutline(RenderBuffer *rb, s32 x, s32 y, s32 halfWidth, s32 halfHeight, u32 color, s32 thickness)
{
    // TODO: Remove corner overdraw ?
    // TopBar:
    RenderBufferDrawQuad(rb, x, y + halfHeight, halfWidth + thickness, thickness, color);
    // RightBar:
    RenderBufferDrawQuad(rb, x + halfWidth, y, thickness, halfHeight + thickness, color);
    // BottomBar:
    RenderBufferDrawQuad(rb, x, y - halfHeight, halfWidth + thickness, thickness, color);
    // LeftBar:
    RenderBufferDrawQuad(rb, x - halfWidth, y, thickness, halfHeight + thickness, color);
}

inline void Swap(s32 *a, s32 *b)
{
    s32 tmp = *a;
    *a = *b;
    *b = tmp;
}

inline void RenderBufferPlot(RenderBuffer *rb, s32 x, s32 y, f32 brightness, u32 color)
{
    if (x >= 0 && x < rb->width && y >= 0 && y < rb->height)
        *((u32 *)rb->memory + y * rb->width + x) = (u32)(brightness * color);
}

inline f32 ipart(f32 x) { return (f32)floor(x); }
inline f32 fpart(f32 x) { return x - (f32)floor(x); }
inline f32 rfpart(f32 x) { return 1 - fpart(x); }

// NOTE(pf): Based on: https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
static void RenderBufferDrawLineAA(RenderBuffer *rb, s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    b32 steep = abs(y1 - y0) > abs(x1 - x0);

    if (steep)
    {
        Swap(&x0, &y0);
        Swap(&x1, &y1);
    }

    if (x0 > x1)
    {
        Swap(&x0, &x1);
        Swap(&y0, &y1);
    }

    f32 dx = (f32)(x1 - x0);
    f32 dy = (f32)(y1 - y0);

    f32 gradient = 1.0f;
    if (dx > EPSILON)
    {
        gradient = dy / dx;
    }

    // first endpoint.

    f32 xend = (f32)round(x0);
    f32 yend = y0 + gradient * (xend - x0);
    f32 xgap = rfpart(x0 + 0.5f);
    f32 xpxl1 = xend;
    f32 ypxl1 = (f32)floor(yend);

    if (steep)
    {
        RenderBufferPlot(rb, (s32)ypxl1, (s32)xpxl1, rfpart(yend) * xgap, color);
        RenderBufferPlot(rb, (s32)ypxl1 + 1, (s32)xpxl1, fpart(yend) * xgap, color);
    }
    else
    {
        RenderBufferPlot(rb, (s32)xpxl1, (s32)ypxl1, rfpart(yend) * xgap, color);
        RenderBufferPlot(rb, (s32)xpxl1, (s32)ypxl1 + 1, fpart(yend) * xgap, color);
    }
    f32 intery = yend + gradient;

    xend = (f32)round(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5f);
    f32 xpxl2 = xend;
    f32 ypxl2 = ipart(yend);

    if (steep)
    {
        RenderBufferPlot(rb, (s32)ypxl2, (s32)xpxl2, rfpart(yend) * xgap, color);
        RenderBufferPlot(rb, (s32)ypxl2 + 1, (s32)xpxl2, fpart(yend) * xgap, color);
    }
    else
    {
        RenderBufferPlot(rb, (s32)xpxl2, (s32)ypxl2, rfpart(yend) * xgap, color);
        RenderBufferPlot(rb, (s32)xpxl2, (s32)ypxl2 + 1, rfpart(yend) * xgap, color);
    }

    // main loop
    if (steep)
    {
        for (s32 x = (s32)xpxl1 + 1; x < xpxl2 - 1; ++x)
        {
            RenderBufferPlot(rb, (s32)ipart(intery), x, rfpart(intery), color);
            RenderBufferPlot(rb, (s32)ipart(intery) + 1, x, fpart(intery), color);
            intery += gradient;
        }
    }
    else
    {
        for (s32 x = (s32)xpxl1 + 1; x < xpxl2 - 1; ++x)
        {
            RenderBufferPlot(rb, x, (s32)ipart(intery), rfpart(intery), color);
            RenderBufferPlot(rb, x, (s32)ipart(intery) + 1, fpart(intery), color);
            intery += gradient;
        }
    }
}

static void RenderBufferDrawEntity(RenderBuffer *rb, Entity *entity)
{
    switch (entity->collisionType)
    {
    case PLAYER:
    {
        f32 lineLength = 50.0f;
        if (v2fLength(&entity->dir) > EPSILON)
        {
            v2f xy1 = v2fAdd(entity->pos, v2fMul(entity->dir, lineLength));
            RenderBufferDrawLineAA(rb, (s32)entity->pos.x, (s32)entity->pos.y, (s32)xy1.x, (s32)xy1.y, RGB(255, 0, 0));
        }

        if (entity->invulnearableTimer_u > 0.0f)
        {
            f32 freq = 4.0f;
            s32 blinking = (s32)(255 * MapIntoRange01F32((f32)cos(freq * entity->invulnearableTimer_u), -1, 1));
            RenderBufferDrawQuadOutline(rb, (s32)entity->pos.x, (s32)entity->pos.y, (s32)entity->halfDim.x, (s32)entity->halfDim.y,
                                        RGB(blinking, blinking, 0), 4);
        }

    } // LET IT FALL THROUGH!
    case ASTEROID:
    case ALIEN:
    {
        RenderBufferDrawQuad(rb, (s32)entity->pos.x, (s32)entity->pos.y, (s32)entity->halfDim.x, (s32)entity->halfDim.y, entity->color);
    }
    break;
    case BULLET:
    {
        RenderBufferDrawCircle(rb, (s32)entity->pos.x, (s32)entity->pos.y, (s32)entity->halfDim.x, entity->color);
    }
    break;
        INVALID_DEFAULT_CASE
    }
}

static v2s GetTextDimensions(GameState *gameState, f32 txtScale, char *text)
{
    v2s result;
    result.x = 0;
    result.y = 0;
    f32 pixelScale = stbtt_ScaleForPixelHeight(&gameState->font, txtScale);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&gameState->font, &ascent, &descent, &lineGap);
    result.y = ascent + descent + lineGap;
    while (*text)
    {
        int advance;
        char c = *text;
        int w, h;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&gameState->font, 0,
                                                         pixelScale, c, &w, &h, 0, 0);
        // .. kerning relative to other characters in the text.
        stbtt_GetCodepointHMetrics(&gameState->font, c, &advance, 0);
        result.x += advance;
        if (c)
        {
            result.x += (s32)(stbtt_GetCodepointKernAdvance(&gameState->font, c, *(text + 1)));
        }
        ++text;
    }

    result = v2sMul(result, pixelScale);
    return result;
}

static void RenderBufferDrawText(RenderBuffer *rb, s32 x, s32 y, GameState *gameState,
                                 f32 txtScale, u32 color, char *text)
{
    f32 pixelScale = stbtt_ScaleForPixelHeight(&gameState->font, txtScale);
    while (*text)
    {
        int advance, offsetX, offsetY, drawY, drawX;
        char c = *text;
        int w, h, i, j;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&gameState->font, 0,
                                                         pixelScale, c, &w, &h, &offsetX, &offsetY);
        offsetY += h;
        drawX = CONTAIN(offsetX + x, 0, rb->width);
        drawY = CONTAIN(y - offsetY, 0, rb->height);
        u32 *drawPtr = ((u32 *)gameState->rb->memory) + (drawY * gameState->rb->width) + drawX;

        // NOTE: We are mapping glyphs into our buffer, bounds check
        // in for are for glyph map, we need extra guards within for
        // our drawbuffer.
        for (j = h - 1;
             j >= 0 && drawY < rb->height;
             --j, ++drawY)
        {
            for (i = 0;
                 (i < w) && drawX < rb->width;
                 ++i, ++drawX)
            {
                if (bitmap[j * w + i])
                {
                    *(drawPtr + i) = color;
                }
            }
            drawX -= i;
            drawPtr += gameState->rb->width;
        }

        // .. kerning relative to other characters in the text.
        stbtt_GetCodepointHMetrics(&gameState->font, c, &advance, 0);
        x += (s32)(advance * pixelScale);
        if (c)
        {
            x += (s32)(pixelScale * stbtt_GetCodepointKernAdvance(&gameState->font, c, *(text + 1)));
        }

        if (x >= rb->width)
            break;

        ++text;
    }
}

b32 KeyStateRelesed(KeyState *state, u32 key)
{
    return state[key].currState == KS_UP && state[key].prevState == KS_DOWN;
}

b32 KeyStateDown(KeyState *state, u32 key)
{
    return state[key].currState == KS_DOWN;
}

b32 KeyStatePressed(KeyState *state, u32 key)
{
    return state[key].currState == KS_DOWN && state[key].prevState == KS_UP;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYUP:
    {
        keyStates[(u8)(wParam)].currState = KS_UP;
    }
    break;
    case WM_KEYDOWN:
    {
        keyStates[(u8)(wParam)].currState = KS_DOWN;
    }
    break;
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

inline u64 Win32QueryPerformanceCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

static inline u64 Win32QueryPerformanceFrequency()
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    return li.QuadPart;
}

static void GameMenuLogic(GameState *gameState)
{
    if (KeyStatePressed(keyStates, 'P'))
    {
        gameState->mode = GS_GAME_SETUP;
    }
    else
    {
        f32 txtScale = 100;
        v2s txtDim = GetTextDimensions(gameState, txtScale, "START MENU");
        RenderBufferDrawText(gameState->rb, (s32)((gameState->screenDim.x / 2.0f) - (txtDim.x / 2.0f)),
                             (s32)(gameState->screenDim.y / 1.0f) - 100,
                             gameState,
                             txtScale, RGB(255, 255, 0), "START MENU");
        txtDim = GetTextDimensions(gameState, txtScale, "PRESS (P) to Play");
        RenderBufferDrawText(gameState->rb, (s32)((gameState->screenDim.x / 2.0f) - (txtDim.x / 2.0f)),
                             (s32)(gameState->screenDim.y / 1.0f) - 100 - txtDim.y,
                             gameState,
                             txtScale, RGB(255, 255, 0), "PRESS (P) to Play");
    }
}

static void GameLogic(GameState *gameState)
{
    // TODO: U know what to do :)
    f32 frameDt = gameState->frameDt;
    Entity *player = gameState->player;
    RenderBuffer *rb = gameState->rb;
    v2s screenDim = gameState->screenDim;
    HWND hwnd = gameState->hwnd;

    v2f center = {screenDim.x / 2.f, screenDim.y / 2.f};
    v2f minPos = {0.0f, 0.0f};
    v2f maxPos = {(f32)screenDim.x, (f32)screenDim.y};
    v2f asteroidDim = {30, 30};

    if (gameState->mode == GS_GAME_SETUP)
    {
        gameState->mode = GS_GAME;
        gameState->score = 0;
        gameState->asteroidCount = 0;
        gameState->asteroidMaxCount = 5;
        gameState->asteroidSpawnTimer = RandomF32Between(2.0f, 5.0f);
        gameState->freeList = 0;
        gameState->activeEntityList = 0;
        gameState->entityCount = 0;
        gameState->player = PlayerCreate(gameState, center);
    }
    else
    {
        // Game Paused.
        if (KeyStatePressed(keyStates, 'P'))
        {
            gameState->mode = GS_PAUSED;
        }

        if (gameState->asteroidCount < gameState->asteroidMaxCount)
        {
            gameState->asteroidSpawnTimer -= frameDt;
            if (gameState->asteroidSpawnTimer <= 0.0f)
            {
                AsteroidCreate(gameState, RandomV2fBetween(minPos, maxPos), asteroidDim, AS_BIG);
                gameState->asteroidSpawnTimer = RandomF32Between(2.0f, 5.0f);
            }
        }

        // TODO: AlienZ.
        // if(gameState->alienCount < gameState->asteroidCount

        v2f ddP = {0, 0};
        if (KeyStateDown(keyStates, VK_LEFT))
        {
            ddP.x -= 1.0f;
        }
        if (KeyStateDown(keyStates, VK_RIGHT))
        {
            ddP.x += 1.0f;
        }
        if (KeyStateDown(keyStates, VK_UP))
        {
            ddP.y += 1.0f;
        }
        if (KeyStateDown(keyStates, VK_DOWN))
        {
            ddP.y -= 1.0f;
        }

        if (KeyStatePressed(keyStates, VK_SPACE))
        {
            f32 bulletSpeed = 10.0f;
            v2f bulletHalfDim = {5.0f, 5.0f};
            v2f spawnPos = v2fAdd(player->pos, v2fHadamard(v2fMul(player->halfDim, 0.5f), player->dir));
            Entity *bullet = EntityCreate(gameState, RGB(0, 255, 0), spawnPos, bulletHalfDim, ASTEROID | ALIEN, BULLET, bulletSpeed);
            bullet->dP = v2fAdd(bullet->dP, v2fMul(player->dir, bulletSpeed));
        }

        if (!v2fIsZero(&ddP))
        {
            v2fNormalized(&ddP);
            player->dP = v2fAdd(player->dP, ddP);
        }

        v2f dPDrag = player->dP;
        v2fNormalized(&dPDrag);
        dPDrag = v2fMul(dPDrag, -0.2f);
        if (v2fMagnitude(&player->dP) > v2fMagnitude(&dPDrag))
        {
            player->dP = v2fAdd(player->dP, dPDrag);
        }
        else
        {
            player->dP.x = 0.0f;
            player->dP.y = 0.0f;
        }

        for (Entity *entity = gameState->activeEntityList;
             entity;
             entity = entity->next)
        {
            v2fContain(&entity->dP, v2fMul(v2fONE, -entity->maxVel), v2fMul(v2fONE, entity->maxVel));
            entity->pos = v2fAdd(entity->pos, entity->dP);
            if (!v2fIsZero(&entity->dP))
            {
                entity->dir = entity->dP;
                v2fNormalized(&entity->dir);
            }

            Entity *collider = EntityCollidesWithAnything(entity, gameState->activeEntityList);
            if (collider)
            {
                if (entity->invulnearableTimer_u <= 0.0f)
                    --entity->health;

                switch (entity->collisionType)
                {
                case ASTEROID:
                {
                    if (entity->stage == AS_BIG)
                    {
                        AsteroidCreate(gameState, entity->pos, v2fMul(entity->halfDim, 0.5f), AS_SMALL);
                        AsteroidCreate(gameState, entity->pos, v2fMul(entity->halfDim, 0.5f), AS_SMALL);
                    }

                    EntityFree(entity, gameState);
                    --gameState->asteroidCount;
                    if (collider->collisionType == BULLET)
                    {
                        ++gameState->score;
                        EntityFree(collider, gameState);
                    }
                    continue;
                }
                break;
                case PLAYER:
                {
                    if (entity->health <= 0)
                    {
                        gameState->mode = GS_GAME_OVER;
                    }
                    else
                    {
                        entity->invulnearableTimer_u = (f32)PI * 2.f;
                    }
                }
                break;
                default:
                {
                    // Do nothing.
                }
                break;
                }
            }

            // MAP / RENDER...
            EntityWrapToWorld(entity, 0, 0, screenDim.x, screenDim.y);
            RenderBufferDrawEntity(rb, entity);
            // .. Check lifetime, this could have been an entity type param specifics, i.e polymorphism..
            entity->lifeTime_u -= frameDt;
            if (entity->collisionType == BULLET && entity->lifeTime_u <= 0.0f)
            {
                EntityFree(entity, gameState);
            }
            else if (entity->invulnearableTimer_u < 0.0f)
            {
                entity->invulnearableTimer_u = 0.0f;
            }
        }

        f32 txtScale = 40.0f;
        char txt[256];
        sprintf(txt, "SCORE: %u", gameState->score);
        RenderBufferDrawText(gameState->rb, 0, 0, gameState, txtScale, RGB(0, 255, 0), txt);

        v2s dims = GetTextDimensions(gameState, txtScale, txt);
        sprintf(txt, "HEALTH: %u", player->health);
        RenderBufferDrawText(gameState->rb, 0, dims.y, gameState, txtScale, RGB(0, 255, 0), txt);
    }
}

static void GamePausedLogic(GameState *gameState)
{
    if (KeyStatePressed(keyStates, 'P'))
    {
        gameState->mode = GS_GAME;
    }
}

static void GameOverLogic(GameState *gameState)
{
    if (KeyStatePressed(keyStates, 'C'))
    {
        gameState->mode = GS_MENU;
    }
    else
    {
        RenderBufferDrawText(gameState->rb, (s32)(gameState->screenDim.x / 2.0f) - 400,
                             (s32)(gameState->screenDim.y / 2.0f),
                             gameState,
                             100, RGB(255, 255, 0), "GAME OVER");
        RenderBufferDrawText(gameState->rb, (s32)(gameState->screenDim.x / 2.0f) - 400,
                             (s32)(gameState->screenDim.y / 2.0f) + 100,
                             gameState,
                             100, RGB(255, 255, 0), "PRESS (C) to Continue");
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    LPCSTR CLASS_NAME = "Sample";

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Create the window.
    HWND hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,            // Optional window styles.
        CLASS_NAME,                 // Window class
        "Learn to Program Windows", // Window text
        WS_OVERLAPPEDWINDOW,        // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,      // Parent window
        NULL,      // Menu
        hInstance, // Instance handle
        NULL       // Additional application data
    );

    if (hwnd == NULL)
    {
        PostQuitMessage(0);
    }

    ShowWindow(hwnd, nCmdShow);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    f64 start, end, frequency;
    f64 targetMS = 1.0f / 60.f;
    start = (f64)Win32QueryPerformanceCounter();
    frequency = (f64)Win32QueryPerformanceFrequency();

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    WINDOWINFO wi;
    ZeroMemory(&wi, sizeof(wi));
    GetWindowInfo(hwnd, &wi);
    RenderBuffer rb;
    ZeroMemory(&rb, sizeof(rb));
    rb.width = wi.rcClient.right - wi.rcClient.left;
    rb.height = wi.rcClient.bottom - wi.rcClient.top;
    rb.memorySize = rb.width * rb.height * sizeof(u32);
    rb.memory = malloc(rb.memorySize);
    ZeroMemory(rb.memory, rb.memorySize);
    ZeroMemory(&rb.bitsInfo, sizeof(rb.bitsInfo));

    rb.bitsInfo.bmiHeader.biSize = sizeof(rb.bitsInfo.bmiHeader);
    rb.bitsInfo.bmiHeader.biWidth = rb.width;
    rb.bitsInfo.bmiHeader.biHeight = rb.height;
    rb.bitsInfo.bmiHeader.biPlanes = 1;
    rb.bitsInfo.bmiHeader.biBitCount = 32;
    rb.bitsInfo.bmiHeader.biCompression = BI_RGB;

    srand((u32)time(0));

    Entity entityList[ENTITY_COUNT];
    v2f minPos = {-50.0f + (rb.width / 2.f), -50.0f + (rb.height / 2.f)};
    v2f maxPos = {50.0f + (rb.width / 2.f), 50.0f + (rb.height / 2.f)};
    v2f dim = {50, 50};

    b32 isRunning = 1;
    GameState gameState;
    gameState.frameDt = 0.0f;
    gameState.rb = &rb;
    gameState.screenDim.x = rb.width;
    gameState.screenDim.y = rb.height;
    gameState.hwnd = hwnd;
    gameState.mode = GS_MENU;
    gameState.entityList = entityList;

    char *ttf_buffer = malloc(sizeof(char) * 1 << 25);
    fread(ttf_buffer, 1, 1 << 25, fopen("c:/windows/fonts/arial.ttf", "rb"));
    stbtt_InitFont(&gameState.font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));

    // GAME LOOP :
    char *text = "GAME ENGINE!!";
    while (isRunning)
    {
        // MSG PUMP:
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // INPUT :
        if (KeyStatePressed(keyStates, VK_ESCAPE))
        {
            isRunning = 0;
        }

        // LOGIC:
        switch (gameState.mode)
        {

        case GS_MENU:
        {
            GameMenuLogic(&gameState);
        }
        break;

        case GS_GAME_SETUP:
        case GS_GAME:
        {
            GameLogic(&gameState);
        }
        break;

        case GS_PAUSED:
        {
            GamePausedLogic(&gameState);
        }
        break;

        case GS_GAME_OVER:
        {
            GameOverLogic(&gameState);
        }
        break;
        default:
        {
            ASSERT(0);
        }
        break;
        }

        // RENDER :
        if(gameState.mode != GS_PAUSED)
        {
            int lines = StretchDIBits(GetDC(hwnd), 0, 0, gameState.screenDim.x, gameState.screenDim.y,
                                        0, 0, gameState.screenDim.x, gameState.screenDim.y,
                                        gameState.rb->memory, &gameState.rb->bitsInfo,
                                        DIB_RGB_COLORS, SRCCOPY);
            RenderBufferClear(gameState.rb);
        }
        for (int i = 0; i < ARRAY_COUNT(keyStates); ++i)
        {
            keyStates[i].prevState = keyStates[i].currState;
        }
        f64 deltaMS;
        do
        {
            end = (f64)Win32QueryPerformanceCounter();
            deltaMS = (f64)(end - start) / (f64)frequency;
            if (targetMS > deltaMS)
            {
                DWORD sleepMS = (DWORD)((targetMS - deltaMS) * 500.0);
                // char s[256];
#pragma warning(disable : 4996)
                // sprintf(s, "Sleeping for: %d ms\n", sleepMS);
                // OutputDebugString(s);
                Sleep(sleepMS);
            }
        } while (deltaMS <= targetMS);
        gameState.frameDt = (f32)deltaMS;
        start = end;
    }
    return 0;
}

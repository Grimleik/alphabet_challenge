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

/* TODO:
 * Bullets!
 * AliEnZ!
 */

#define ASSERT(x) if(!(x))                      \
    {                                           \
        DebugBreak();                           \
    }                                           \

#define INVALID_DEFAULT_CASE default: { ASSERT(1); }
#undef RGB
#define RGB(r, g, b) (u32)((r) << 16 | (g) << 8 | (b))
enum KEY_STATE
{
    KS_UP,
    KS_DOWN,
};

enum GAME_STATE
{
    MENU,
    GAME_SETUP,
    GAME,
    PAUSED,
    GAME_OVER,
};

enum COLLISION_TYPE
{
    BULLET = 0x0,
    PLAYER = 0x1,
    ASTEROID = 0x2,
    ALIEN = 0x4,
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

typedef struct Entity
{
    v2f pos;
    v2f dP;
    v2f dim;
    u32 color;

    u32 collisionMask;
    u32 collisionType;
    u32 health;
    
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
    b32 isPaused;
    u32 score;
        
    f32 shootingDelay;
    f32 maxShootingDelay;

    stbtt_fontinfo font;

} GameState;

// GLOBALS:
struct KeyState keyStates[0xFE];
#define ENTITY_COUNT 256


inline f32 RandomNormalized()
{
    return (f32)(rand()) / RAND_MAX;
}

#define RANDOM_BETWEEN(name, type)                               \
    inline type Random##name##Between(type min, type max)        \
    {                                                           \
        return (RandomNormalized() * (max - min)) + min;       \
    }

// NOTE(pf): CBA To create a bunch of math util functions atm, just
// here to test the possibilities of macros to create generic
// functions for the future.
RANDOM_BETWEEN(F32, f32)
// RANDOM_BETWEEN(V2f, v2f) NOTE: No operator overloading ):

inline v2f RandomV2fBetween(v2f min, v2f max)
{
    v2f s0 = v2fSub(max, min);
    v2f s1 = v2fAdd(s0, min);
    v2f s2 = {RandomNormalized() * 2.0f - 1.0f, RandomNormalized() * 2.0f - 1.0f};
    // v2fNormalize(&s2);
    v2f result = v2fHadamard(s2, s1);
    return result;
}

// TODO: Test default args.
inline u32 RandomColorBetween(u8 minR, u8 minG, u8 minB,
                              u8 maxR, u8 maxG, u8 maxB)
{
    u8 r = (u8)(RandomNormalized() * (maxR - minR)) + minR;
    u8 g = (u8)(RandomNormalized() * (maxG - minG)) + minG;
    u8 b = (u8)(RandomNormalized() * (maxB - minB)) + minB;
    u32 result = r << 16 | g << 8 | b;
    return result;
}

inline v2f RandomPositionBetween(v2f minP, v2f maxP)
{
    v2f result;
    result.x = RandomNormalized() * (maxP.x - minP.x) + minP.x;
    result.y = RandomNormalized() * (maxP.y - minP.y) + minP.y;
    return result;
}

inline v2f RandomDimensionBetween(v2f minP, v2f maxP)
{
    v2f result;
    result.x = RandomNormalized() * (maxP.x - minP.x) + minP.x;
    result.y = RandomNormalized() * (maxP.y - minP.y) + minP.y;
    return result;
}

static Entity *EntityCreate(GameState *gameState, u32 color, v2f pos, v2f dim,
                            u32 collisionMask, u32 collisionType)
{
    Entity *result;
    if(gameState->freeList)
    {
        result = gameState->freeList;
        gameState->freeList = gameState->freeList->next;
        if(gameState->freeList)
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
    result->dim = dim;
    result->collisionMask = collisionMask;
    result->collisionType = collisionType;
    result->health = 1;
    result->prev = result->next = 0;

    if(gameState->activeEntityList)
    {
        // result->prev = gameState->activeEntityList->prev;
        result->next = gameState->activeEntityList;
        gameState->activeEntityList->prev = result;
    }
    
    gameState->activeEntityList = result;
    return result;
}

// TODO: This needs another pass.
static void EntityFree(Entity *entity, GameState *gameState)
{
    if(gameState->activeEntityList == entity)
    {
        gameState->activeEntityList = entity->next;
    }
    else
    {
        ASSERT(entity->next || entity->prev);
        if(entity->next)
            entity->next->prev = entity->prev;
        if(entity->prev)
            entity->prev->next = entity->next;
    }
    
    if(gameState->freeList)
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
    if(a->collisionMask & b->collisionType)
    {
        v2f pos = v2fSub(b->pos, a->pos);
        v2f minkowskiDim;
        minkowskiDim.x = (a->dim.x + b->dim.x) / 2;
        minkowskiDim.y = (a->dim.y + b->dim.y) / 2;
        
        if(pos.x >= -minkowskiDim.x && pos.x < minkowskiDim.x &&
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
    for(Entity *collider = entityList;
        collider;
        collider = collider->next)
    {
        if(entity != collider &&
           EntityCollidesWith(entity, collider))
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

static void RenderBufferDrawQuad(RenderBuffer *rb, s32 x, s32 y, s32 width, s32 height, u32 color)
{
    s32 drawX = CONTAIN(x, 0, rb->width);
    s32 drawY = CONTAIN(y, 0, rb->height);
    u32 drawWidth = CONTAIN(x + width, 0, rb->width);
    u32 drawHeight = CONTAIN(y + height, 0, rb->height);
    for (u32 itY = drawY; itY < drawHeight; ++itY)
    {
        for (u32 itX = drawX; itX < drawWidth; ++itX)
        {
            u32 *pixel = (u32 *)rb->memory + ((rb->width * itY) + itX);
            *pixel = color;
        }
    }
}
static void RenderBufferDrawEntity(RenderBuffer *rb, Entity *entity)
{
    RenderBufferDrawQuad(rb, (s32)entity->pos.x, (s32)entity->pos.y, (s32)entity->dim.x, (s32)entity->dim.y, entity->color);
}

static void RenderBufferDrawText(RenderBuffer *rb, s32 x, s32 y, GameState *gameState,
                                 f32 s, u32 color, char *text)
{
    // TODO: Bounds checking.
    f32 xpos = 0.0f;
    while(*text)
    {
        int advance, lsb;
        u32 *bb = ((u32*)gameState->rb->memory) + (rb->width * y) + x + ((u32)floor(xpos));
        char c = *text;
        int w,h,i,j;
        f32 pixelScale = stbtt_ScaleForPixelHeight(&gameState->font, s);
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&gameState->font, 0,
                                                         pixelScale, c, &w, &h, 0,0);
        for (j=h - 1; j >= 0; --j)
        {
            for (i=0; i < w; ++i)
            {
                if(bitmap[j*w + i])
                {
                    *(bb + i) = color;
                }
            }
            bb += gameState->rb->width;
        }
        float subScale = pixelScale;
        float x_shift = xpos - (float) floor(xpos);
        stbtt_GetCodepointHMetrics(&gameState->font, c, &advance, &lsb);
        xpos += (advance * subScale);
        if (c)
            xpos += subScale*stbtt_GetCodepointKernAdvance(&gameState->font, c, *(text + 1));
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
    if(KeyStatePressed(keyStates, 'P'))
    {
        gameState->mode = GAME_SETUP;
    }
    else
    {
        RenderBufferDrawText(gameState->rb, (s32)(gameState->screenDim.x / 2.0f) - 400,
                             (s32)(gameState->screenDim.y / 2.0f),
                             gameState,
                             100, RGB(255, 255, 0), "START MENU");
        RenderBufferDrawText(gameState->rb, (s32)(gameState->screenDim.x / 2.0f) - 400,
                             (s32)(gameState->screenDim.y / 2.0f) + 100,
                             gameState,
                             100, RGB(255, 255, 0), "PRESS (P) to Play");
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
    
    v2f minPos = {-50.0f + (screenDim.x / 2.f), -50.0f + (screenDim.y / 2.f)};
    v2f maxPos = {50.0f + (screenDim.x / 2.f), 50.0f + (screenDim.y / 2.f)};
    v2f dim = {50, 50};
    
    if(gameState->mode == GAME_SETUP)
    {
        gameState->mode = GAME;
        gameState->score = 0;
        gameState->asteroidCount = 0;
        gameState->asteroidMaxCount = 25;
        gameState->asteroidSpawnTimer = RandomF32Between(1.0f, 3.0f);
        gameState->freeList = 0;
        gameState->activeEntityList = 0;
        gameState->entityCount = 0;
        gameState->player = EntityCreate(gameState, RGB(255, 255, 255),
                                         RandomPositionBetween(minPos, maxPos),
                                         RandomDimensionBetween(dim, dim),
                                         ASTEROID | ALIEN, PLAYER);
    }
    else
    {
        // Game Paused.
        if(KeyStatePressed(keyStates, 'P'))
        {   
            gameState->mode = PAUSED;
        }

        if(gameState->asteroidCount < gameState->asteroidMaxCount)
        {
            gameState->asteroidSpawnTimer -= frameDt;
            if(gameState->asteroidSpawnTimer <= 0.0f)
            {
                Entity *asteroid = EntityCreate(gameState,
                                                RandomColorBetween(64, 64, 64, 192, 192, 192),
                                                RandomPositionBetween(minPos, maxPos),
                                                RandomDimensionBetween(dim, dim),
                                                BULLET, ASTEROID);
                v2f minVel = {-2.f, -2.f};
                v2f maxVel = {2.0f, 2.0f};
                asteroid->dP = RandomV2fBetween(minVel, maxVel);
                gameState->asteroidSpawnTimer = RandomF32Between(0.5f, 2.5f);
                gameState->asteroidCount++;
            }
        }
        
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

        if(KeyStateDown(keyStates, VK_SPACE))
        {
            // TODO:
        }

        if (!v2fIsZero(&ddP))
        {
            v2fNormalize(&ddP);
            player->dP = v2fAdd(player->dP, ddP);
        }

        v2f dPDrag = player->dP;
        v2fNormalize(&dPDrag);
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

        for(Entity *entity = gameState->activeEntityList;
            entity;
            entity = entity->next)
        {
            entity->pos = v2fAdd(entity->pos, entity->dP);
            Entity *collider = EntityCollidesWithAnything(entity, gameState->activeEntityList);
            if(collider)
            {
                --entity->health;
                if(entity->health <= 0)
                {

                    switch(entity->collisionType)
                    {
                        case ASTEROID:
                        {
                            // TODO: Break into smaller chunks                            
                            --gameState->asteroidCount;
                            EntityFree(entity, gameState);
                            continue;
                        } break;
                        case BULLET:
                        {
                            ++gameState->score;
                        } break;
                        case PLAYER:
                        {
                            gameState->mode = GAME_OVER;
                        } break;
                        INVALID_DEFAULT_CASE
                    }
                }
            }
        
            EntityWrapToWorld(entity, 0, 0, screenDim.x, screenDim.y);
            RenderBufferDrawEntity(rb, entity);

            if(entity->collisionType == PLAYER)
            {
                RenderBufferDrawQuad(rb, (s32)entity->pos.x, (s32)entity->pos.y, 20, 20, RGB(255, 0, 0));
            }
        }

        char txt[256];
        sprintf(txt, "SCORE: %u", gameState->score);
        RenderBufferDrawText(gameState->rb, 0, 0, gameState, 40.0f, RGB(0, 255, 0), txt);
    }
}

static void GamePausedLogic(GameState *gameState)
{
    if(KeyStatePressed(keyStates, 'P'))
    {   
        gameState->mode = GAME;
    }
}

static void GameOverLogic(GameState *gameState)
{
    if(KeyStatePressed(keyStates, 'C'))
    {
        gameState->mode = MENU;
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
    gameState.mode = MENU;
    gameState.entityList = entityList;

    char *ttf_buffer = malloc(sizeof(char) * 1<<25);
    fread(ttf_buffer, 1, 1<<25, fopen("c:/windows/fonts/arial.ttf", "rb"));
    stbtt_InitFont(&gameState.font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));
    
    
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
        switch (gameState.mode) {

            case MENU:
            {
                GameMenuLogic(&gameState);
            }break;

            case GAME_SETUP:
            case GAME:
            {
                GameLogic(&gameState);
            }break;

            case PAUSED:
            {
                GamePausedLogic(&gameState);
            }break;

            case GAME_OVER:
            {
                GameOverLogic(&gameState);
            }break;
            default:
            {
                ASSERT(0);
            }break;
        }

        // RENDER :
        int lines = StretchDIBits(GetDC(hwnd), 0, 0, gameState.screenDim.x, gameState.screenDim.y,
                                  0, 0, gameState.screenDim.x, gameState.screenDim.y,
                                  gameState.rb->memory, &gameState.rb->bitsInfo,
                                  DIB_RGB_COLORS, SRCCOPY);
        RenderBufferClear(gameState.rb);

    
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

/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#include "age.h"
#include "../externals/fmod/core/inc/fmod.hpp"

struct Player
{
    v2f pos;
    v2f dP;

    v2f dim;
    v4f color;
    f32 acceleration;

    TextureInfo texture;
};

struct Ball
{
    v2f pos;
    v2f dP;

    f32 radius;
    v4f color;
    f32 acceleration;
    Ball *prev;
    Ball *next;

    void Init(v2f spawnPos, f32 radius_, f32 acceleration_ = 20.f, v4f color_ = COLORS::BLUE);
};

void Ball::Init(v2f spawnPos, f32 radius_, f32 acceleration_, v4f color_)
{
    dP = v2f{-1.f, 1.f}.normalize();
    color = color_;
    acceleration = acceleration_;
    radius = radius_;
    pos = spawnPos;
}

struct Block
{
    v2f pos;

    v2f dim;
    v4f color;
    s32 health;
    b32 isBreakable;

    Block *prev;
    Block *next;
};

struct Camera
{
    v2f pos;
    Rectangle2 bounds;
};

struct RectCollisionData
{
    b32 collision;
    v2f point;
    v2f normal;
};

// NOTE: This assumes prev pos. has been checked.
RectCollisionData UnboundedCollision(v2f rectPos, v2f rectDim,
                                     v2f testPos, v2f testDim = {0.0f, 0.0f})
{
    RectCollisionData result = {};
    v2f halfDim = v2f{rectDim + testDim} * 0.5f;

    f32 dx = testPos.x - rectPos.x;
    f32 px = halfDim.x - abs(dx);
    if (px <= 0)
        return result;

    f32 dy = testPos.y - rectPos.y;
    f32 py = halfDim.y - abs(dy);
    if (py <= 0)
        return result;

    result.collision = true;
    if (px < py)
    {
        f32 sx = dx > 0 ? 1.0f : -1.0f;
        result.normal = v2f{sx, 0};
        result.point = {rectPos.x + halfDim.x * sx, testPos.y};
    }
    else
    {
        f32 sy = dy > 0 ? 1.0f : -1.0f;
        result.normal = v2f{0, sy};
        result.point = {testPos.x, rectPos.y + halfDim.y * sy};
    }

    return result;
}

RectCollisionData BoundedCollision(v2f rectPos, v2f rectDim,
                                   v2f testPos, v2f testDim = {0.0f, 0.0f})
{
    RectCollisionData result = {};
    v2f wallMinkowskiDim = (rectDim - testDim) * 0.5f;
    v2f minPoint = rectPos - wallMinkowskiDim;
    v2f maxPoint = rectPos + wallMinkowskiDim;
    if (testPos.x < minPoint.x)
    {
        result.collision = true;
        result.point = {minPoint.x, testPos.y};
        result.normal = {1.0f, 0.0f};
    }
    else if (testPos.x > maxPoint.x)
    {
        result.collision = true;
        result.point = {maxPoint.x, testPos.y};
        result.normal = {-1.0f, 0.0f};
    }
    else if (testPos.y < minPoint.y)
    {
        result.collision = true;
        result.point = {testPos.x, minPoint.y};
        result.normal = {0.0f, 1.0f};
    }
    else if (testPos.y > maxPoint.y)
    {
        result.collision = true;
        result.point = {testPos.x, maxPoint.y};
        result.normal = {0.0f, -1.0f};
    }
    return result;
}

struct App
{
    void Initialize(PlatformState *ps, void *memory, MemoryMarker size);
    void Reload();

    void Update(const f32 deltaTime);

    void Shutdown();

    MemoryStack frameStack;
    MemoryStack permanentStack;

    Input *input;
    RenderList *renderList;

    // STATE:
    Camera camera;
    Player player;
    MemoryPool<Block> blocks;
    MemoryPool<Ball> balls;

    Block *blockSentinel;
    Ball *ballSentinel;
    u32 maxBalls;
    u16 life;
    u16 score;

    b32 isRunning;
    b32 isPaused;
    b32 DEBUGShouldStep;
    b32 DEBUGFixedFrameStep;

    stbtt_fontinfo *arialFont;
    FMOD::System *fmod;
    FMOD::Sound *sound;
    FMOD::Sound *ballBoxCollision;
    FMOD::Sound *ballPlayerCollision;

    FMOD::Channel *soundChannel;
    TextureInfo debugTextureHandle;
};

void App::Initialize(PlatformState *ps, void *memory, MemoryMarker size)
{
    // TODO(pf): Formalize space requirements more than just 1/4 and 3/4.
    MemoryMarker frameSize = (MemoryMarker)(0.25f * size);
    frameStack.Init(memory, frameSize);
    void *perm = ((u8 *)memory + frameSize);
    permanentStack.Init(perm, size - frameSize);

    renderList = ps->renderList;
    input = ps->input;

    camera.bounds.dim.x = renderList->windowWidth / renderList->metersToPixels;
    camera.bounds.dim.y = renderList->windowHeight / renderList->metersToPixels;
    camera.pos = {0.0f, 0.0f};

    player.pos = {0.0f, -camera.bounds.dim.y * 0.5f + player.dim.y * 0.5f};
    player.color = COLORS::ORANGE;
    player.acceleration = 100.f;
    player.texture = LoadTexture("../../assets/textures/player.png");
    player.dim =
        v2f{
            player.texture.width / renderList->metersToPixels,
            player.texture.height / renderList->metersToPixels,
        };
    // player.dim = {4, 0.5f};

    maxBalls = 10;
    balls.PartitionFrom(&permanentStack, maxBalls + 1);
    balls.Init();
    ballSentinel = balls.AllocateAndInit();
    ballSentinel->prev = ballSentinel->next = ballSentinel;

    s32 blocksPerLine = 8, lines = 4, nrOfBlocks = 1 + blocksPerLine * lines;
    blocks.PartitionFrom(&permanentStack, nrOfBlocks);
    blocks.Init();

    v2f blockDims = {2.0f, 0.5f};
    v2f blockMargin = {0.1f, 0.1f};
    v2f firstBlockPos = {-blockDims.x * blocksPerLine * 0.5f + blockDims.x * 0.5f,
                         blockDims.y * 0.5f + 6.0f};

    blockSentinel = blocks.Allocate();
    Block *b = blockSentinel;
    for (s32 i = 0; i < nrOfBlocks - 1; ++i)
    {
        Block *next = blocks.AllocateAndInit();
        next->color = COLORS::GREEN;
        next->dim = blockDims - blockMargin;
        next->pos = firstBlockPos + v2f{(i % blocksPerLine) * blockDims.x,
                                        -(i / blocksPerLine) * blockDims.y};
        next->prev = b;
        b->next = next;
        b = next;
    }
    blockSentinel->prev = b;
    blockSentinel->prev->next = blockSentinel;
    arialFont = LoadFont("c:/windows/fonts/arial.ttf", &permanentStack);

    FMOD_RESULT fr;
    fr = FMOD::System_Create(&fmod);
    Assert(fr == FMOD_OK);
    fr = fmod->init(512, FMOD_INIT_NORMAL, 0);
    Assert(fr == FMOD_OK);
    fr = fmod->createSound("../../assets/sounds/sound.ogg", FMOD_DEFAULT, NULL, &sound);
    Assert(fr == FMOD_OK);
    fr = fmod->createSound("../../assets/sounds/collision.ogg", FMOD_DEFAULT, NULL, &ballPlayerCollision);
    Assert(fr == FMOD_OK);
    fr = fmod->createSound("../../assets/sounds/collision.ogg", FMOD_DEFAULT, NULL, &ballBoxCollision);
    Assert(fr == FMOD_OK);

    debugTextureHandle = LoadTexture("../../assets/textures/debug_uv.jpg");
    Assert(debugTextureHandle.memory);

    isRunning = true;
}

void App::Reload()
{
}

void App::Update(f32 deltaTime)
{
    frameStack.Clear();
    if (input->KeyPressed(KEY::P))
    {
        isPaused = !isPaused;
    }
    if(input->KeyPressed(KEY::X))
    {
        DEBUGFixedFrameStep = !DEBUGFixedFrameStep;
    }
    if(DEBUGFixedFrameStep)
    {
        deltaTime = 0.016f;
    }
    if (isPaused)
    {
        if (input->KeyPressed(KEY::M))
        {
            DEBUGShouldStep = true;
        }
    }
    if (!isPaused || DEBUGShouldStep)
    {
        DEBUGShouldStep = false;
        if (input->KeyPressed(KEY::ESCAPE))
        {
            isRunning = false;
        }

        if (input->KeyPressed(KEY::V))
        {
            renderList->settings->isVsync = !renderList->settings->isVsync;
        }

        if (input->KeyPressed(KEY::F))
        {
            renderList->settings->shouldToggleFullscreen = true;
        }

        if (input->KeyDown(KEY::R))
        {
            player.pos = {0.0f, camera.pos.y + player.dim.y * 0.5f};
            player.dP = {0.0f, 0.0f};
        }

        if (input->KeyPressed(KEY::N))
        {
            bool res;
            soundChannel->isPlaying(&res);
            if (!res)
            {
                fmod->playSound(sound, NULL, false, &soundChannel);
            }
        }

        if (ballSentinel->next == ballSentinel && input->KeyPressed(KEY::SPACE))
        {
            ballSentinel->next = balls.AllocateAndInit();
            ballSentinel->next->Init(player.pos + v2f{0.0f, player.dim.y * 0.6f + 0.125f}, 0.25f);
            ballSentinel->next->prev = ballSentinel;
            ballSentinel->next->next = ballSentinel;
        }

        v2f ddP = {0.0f, 0.0f};
        if (input->KeyDown(KEY::A) ||
            input->KeyDown(KEY::ARROW_LEFT))
        {
            ddP += {-1.0f, 0.0f};
        }
        if (input->KeyDown(KEY::D) ||
            input->KeyDown(KEY::ARROW_RIGHT))
        {
            ddP += {1.0f, 0.0f};
        }

        if (input->KeyDown(KEY::W) ||
            input->KeyDown(KEY::ARROW_UP))
        {
            player.pos.y += 0.01f * (input->KeyDown(KEY::Z) ? 10.0f : 1.0f);
        }
        if (input->KeyDown(KEY::S) ||
            input->KeyDown(KEY::ARROW_DOWN))
        {
            player.pos.y -= 0.01f * (input->KeyDown(KEY::Z) ? 10.0f : 1.0f);
        }

        f32 MAX_VEL = 10.0f;
        if (ddP.length2() > 0)
        {
            ddP = player.acceleration * ddP.normalize();
        }

        v2f newDp = player.dP + deltaTime * ddP;
        f32 length = newDp.length();
        if (length > MAX_VEL)
        {
            newDp = newDp.normalize() * MAX_VEL;
        }

        // RENDER AND UPDATE START:

        RenderGroup *rg = renderList->PushRenderGroup();
        rg->m_offset = camera.pos - 0.5f * camera.bounds.dim;
        RCClearColor *rccs = rg->PushCommand<RCClearColor>();
        rccs->color = COLORS::BLACK;

#if 1
        RCDrawTexture *rcdt = rg->PushCommand<RCDrawTexture>();
        rcdt->color_tint = COLORS::BLACK;
        rcdt->pos = {0.0f, 0.0f};
        rcdt->info = &debugTextureHandle;
#endif
        // TODO: More movement tuning and reading about physics.
        // Cylinder coefficient, air density, 1/2, gameification constant & dt
        f32 gameGlideCoefficient = 11.0f;
        f32 dragC = 0.8f * 1.001293f * 0.5f * gameGlideCoefficient * deltaTime;
        v2f drag = {newDp.x, newDp.y};
        newDp.x -= dragC * drag.x;

        // s = ut + 0.5at^2;
        v2f newPos = player.pos + (newDp * deltaTime) +
                     0.5f * (ddP * deltaTime * deltaTime);

        RectCollisionData col = BoundedCollision(camera.pos, camera.bounds.dim, newPos, player.dim);
        if (col.collision)
        {
            player.pos = col.point;
            player.dP = {};
        }
        else
        {
            player.pos = newPos;
            player.dP = newDp;
        }
        for (Ball *ball = ballSentinel->next; ball != ballSentinel; ball = ball->next)
        {
            v2f newBallPos = ball->pos + ball->dP * deltaTime + 0.5f * ball->dP * ball->acceleration * deltaTime;
            col = BoundedCollision(camera.pos, camera.bounds.dim, newBallPos, v2f{ball->radius * 2.0f, ball->radius * 2.0f});
            if (col.collision)
            {
                // OUTSIDE LOWER PART ? GAME OVER
                if (newBallPos.y < col.point.y)
                {
                    ball->prev->next = ball->next;
                    ball->next->prev = ball->prev;
                    balls.Free(ball);
                    --life;
                    // TODO: isRunning = false;
                }
                ball->pos = col.point;
                ball->dP = ball->dP.reflect(col.normal);
            }
            else
            {
                // BALL vs PLAYER
                RectCollisionData ballVsPlayer = UnboundedCollision(player.pos, player.dim, newBallPos,
                                                                    v2f{ball->radius * 2.0f, ball->radius * 2.0f});
                if (ballVsPlayer.collision)
                {
                    fmod->playSound(ballPlayerCollision, NULL, false, NULL);
                    v2f db = (ball->pos - player.pos);
                    ball->dP = db.normalize();
                    newBallPos = ballVsPlayer.point;
                }

                // BALL vs BLOCK
                f32 bestDist = FLT_MAX;
                RectCollisionData closestCollision = {};
                for (Block *block = blockSentinel->next; block != blockSentinel; block = block->next)
                {
                    RectCollisionData ballVsBox = UnboundedCollision(block->pos, block->dim, newBallPos,
                                                                     v2f{ball->radius * 2.0f, ball->radius * 2.0f});
                    if (ballVsBox.collision)
                    {
                        fmod->playSound(ballBoxCollision, NULL, false, NULL);
                        f32 currDist = (ballVsBox.point - newBallPos).length2();
                        if (currDist < bestDist)
                        {
                            closestCollision = ballVsBox;
                            bestDist = currDist;
                        }

                        --block->health;
                        ++score;
                        if (block->health <= 0)
                        {
                            block->prev->next = block->next;
                            block->next->prev = block->prev;
                            blocks.Free(block);
                        }
                        newBallPos = ballVsBox.point;
                    }
                }

                if (closestCollision.collision)
                {
                    ball->dP = ball->dP.reflect(closestCollision.normal);
                    ball->pos = newBallPos;
                }

                ball->pos = newBallPos;

                RCDrawCircle *rcCircle = rg->PushCommand<RCDrawCircle>();
                rcCircle->center = ball->pos;
                rcCircle->radius = ball->radius;
                rcCircle->color = ball->color;

                RCDrawQuadOutline *rcCollisionBox = rg->PushCommand<RCDrawQuadOutline>();
                rcCollisionBox->thickness = 0.05f;
                rcCollisionBox->pos = ball->pos;
                rcCollisionBox->dim = v2f{ball->radius * 2.0f, ball->radius * 2.0f};
                rcCollisionBox->color = COLORS::MAGENTA;
            }
        }

#if 0
        RCDrawQuad *rcPlayer = rg->PushCommand<RCDrawQuad>();
        rcPlayer->color = player.color;
        rcPlayer->pos = player.pos;
        rcPlayer->dim = player.dim;
#else
        RCDrawTexture *rcPlayer = rg->PushCommand<RCDrawTexture>();
        rcPlayer->pos = player.pos;
        rcPlayer->info = &player.texture;
        rcPlayer->color_tint = player.color;
#endif

        for (Block *block = blockSentinel->next; block != blockSentinel; block = block->next)
        {
            RCDrawQuad *rcBlock = rg->PushCommand<RCDrawQuad>();
            rcBlock->color = block->color;
            rcBlock->pos = block->pos;
            rcBlock->dim = block->dim;
        }

        f32 BORDER_WIDTH = 0.1f;
        RCDrawQuad *rcUpperBorder = rg->PushCommand<RCDrawQuad>();
        rcUpperBorder->pos = {0, 0.5f * (camera.bounds.dim.y - BORDER_WIDTH)};
        rcUpperBorder->dim.x = camera.bounds.dim.x;
        rcUpperBorder->dim.y = BORDER_WIDTH;
        rcUpperBorder->color = COLORS::ORANGE;

#if 0
        RCDrawQuad *rcLowerBorder = rg->PushCommand<RCDrawQuad>();
        rcLowerBorder->m_pos = {0, 0.5f * (-camera.bounds.m_dim.y + BORDER_WIDTH)};
        rcLowerBorder->m_dim.x = camera.bounds.m_dim.x;
        rcLowerBorder->m_dim.y = BORDER_WIDTH;
        rcLowerBorder->m_color = COLORS::ORANGE;
#endif
        RCDrawQuad *rcRightBorder = rg->PushCommand<RCDrawQuad>();
        rcRightBorder->pos = {0.5f * (camera.bounds.dim.x - BORDER_WIDTH), 0.0f};
        rcRightBorder->dim.x = BORDER_WIDTH;
        rcRightBorder->dim.y = camera.bounds.dim.y;
        rcRightBorder->color = COLORS::ORANGE;

        RCDrawQuad *rcLeftBorder = rg->PushCommand<RCDrawQuad>();
        rcLeftBorder->pos = {0.5f * (-camera.bounds.dim.x + BORDER_WIDTH), 0.0f};
        rcLeftBorder->dim.x = BORDER_WIDTH;
        rcLeftBorder->dim.y = camera.bounds.dim.y;
        rcLeftBorder->color = COLORS::ORANGE;
#if 0
        RCDrawQuad *rcQuad = rg->PushCommand<RCDrawQuad>();
        rcQuad->pos = {camera.pos.x, camera.pos.y};
        rcQuad->dim = {0.5f, 0.5f};
        rcQuad->color = COLORS::LIGHTGRAY;
#endif

        RCDrawQuadOutline *rcCollisionBox = rg->PushCommand<RCDrawQuadOutline>();
        rcCollisionBox->thickness = 0.05f;
        rcCollisionBox->pos = player.pos;
        rcCollisionBox->dim = player.dim;
        rcCollisionBox->color = COLORS::MAGENTA;

        for (Block *block = blockSentinel->next; block != blockSentinel; block = block->next)
        {
            RCDrawQuadOutline *blockCB = rg->PushCommand<RCDrawQuadOutline>();
            blockCB->thickness = 0.05f;
            blockCB->pos = block->pos;
            blockCB->dim = block->dim;
            blockCB->color = COLORS::MAGENTA;
        }

        RenderGroup *uiRG = renderList->PushRenderGroup();
        uiRG->m_type = RENDERGROUP_TYPE::UI;
        u32 stringSize = 256;
        char *str = (char *)frameStack.AllocateBytesAndZero(sizeof(char) * stringSize);
        sprintf(str, "Score: %u   Life: %u", score, life);
        TextInfo tInfo = {};
        RCDrawText *rcScoreAndLife = uiRG->PushCommand<RCDrawText>();
        rcScoreAndLife->color = COLORS::WHITE;
        rcScoreAndLife->pos = {-0.74f, 0.85f};
        rcScoreAndLife->txtInfo.font = arialFont;
        rcScoreAndLife->txtInfo.txt = (char *)str;
        rcScoreAndLife->txtInfo.scale = 50.0f;
        rcScoreAndLife->txtInfo.centered = true;
    }
    // SOUND
    fmod->update();
}

void App::Shutdown()
{
    fmod->release();
}

GAME_INIT(GameInit)
{
    App *app = (App *)platformState->memory;
    if (reloaded)
    {
        app->Reload();
    }
    else
    {
        app = (App *)platformState->memory;
        *app = {};

        void *memory = ((u8 *)platformState->memory) + sizeof(App);
        app->Initialize(platformState, memory, platformState->memorySize - sizeof(App));
    }
}

GAME_UPDATE(GameUpdate)
{
    App *app = (App *)platformState->memory;
    app->Update(platformState->deltaTime);
    platformState->isRunning = app->isRunning;
    if (!platformState->isRunning)
    {
        app->Shutdown();
    }
}

GAME_SHUTDOWN(GameShutdown)
{
    App *app = (App *)platformState->memory;
    app->Shutdown();
}
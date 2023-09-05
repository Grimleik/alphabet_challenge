/* ========================================================================
   $Creator: Patrik Fjellstedt $
======================================================================== */
#include "age.h"

/* TODO:

 *GAME:

 * Turret Entity.

 *ENGINE:

 * Fonts.

 * Camera Projection (for zooming).

 ** Relative MousePos to screen coordinates, i.e non pixel space cleanup.

 ** Draw Line with thickness (look into breshenham line drawing routine:
    https://github.com/ArminJo/STMF3-Discovery-Demos/blob/master/lib/graphics/src/thickLine.cpp

 ** Screen to World Ray ^.

 * Software drawing clipping.

 FIXME:

 * Current memorypool / free, inuse structure is bad, construct a new
 * type of memorypool that actively tracks memory in use aswell ?

 * Navmesh controls the dimensions of the scene, do we want it to be like that ?


 * PRIO: Need to fix arrival on tile before next path is started!!

 */

const int MAP_WIDTH = 45;
const int MAP_HEIGHT = 45;
const char MAP_ONE[MAP_HEIGHT][MAP_WIDTH] = {
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "WS00001000000000005000000000004000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000002000000000000000000000003000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000006000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000000000W",
    "W000000000000000000000000000000000000070000W",
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"};

const v4f TILE_EMPTY_COLOR = COLORS::DARKGRAY;
const v4f TILE_OCCUPIED_COLOR = COLORS::LIGHTGRAY;
const v4f TILE_PATH_START_COLOR = COLORS::RED;
const v4f TILE_PATH_COLOR = COLORS::GREEN;
const v4f TILE_PATH_TRAVERSED_COLOR = COLORS::YELLOW;
const v4f TILE_PATH_END_COLOR = COLORS::BLUE;

enum NavTileState
{
    EMPTY,
    BLOCKING,

    PATH,
    PATH_START,
    PATH_TRAVERSED,
    PATH_END,
};

struct Link
{
    f32 m_cost = 1.0f;
    struct NavTile *m_target;
};

#define NEIGHBOUR_COUNT 8

struct NavTile
{
    Rectangle2 m_rect;
    v2f m_pos;
    v4f m_color;
    NavTileState m_state;
    Link m_links[NEIGHBOUR_COUNT];

    NavTile *m_parent;
    f32 m_activeCost;

    void Reset()
    {
        m_parent = nullptr;
        m_activeCost = FLT_MAX / 2;
        /*if(m_state != BLOCKING)
            m_state = EMPTY;
        */
    }
};

struct Waypoint
{
    Waypoint() = default;
    NavTile *m_target;
    Waypoint *m_next;
};

struct PathTile
{
    NavTile *m_tile;
    PathTile *m_next;
};

struct Path
{
    b32 IsValid() { return m_start != nullptr && m_end != nullptr; }
    void PushPath(RenderGroup *rg);
    PathTile *m_start;
    PathTile *m_end;
    u32 m_length;
};

struct PathFollower
{
    Path m_path;
    PathTile *m_targetTile;
    v2f m_targetPos;
};

class App;
class NavMesh
{
public:
    void Init(MemoryStack *allocator, u32 tilesPerWidth, u32 tilesPerHeight);
    void Render(App *app, RenderGroup *rg);
    void Clear();

    NavTile *GetTileAt(v2i index)
    {
        u32 arrIndex = index.x + (index.y * m_tilesPerWidth);
        if (arrIndex >= 0 && arrIndex <= (m_tilesPerWidth * m_tilesPerHeight))
        {
            return m_tiles + arrIndex;
        }

        return nullptr;
    }

    NavTile *GetTileAt(v2f pos)
    {
        u32 x = (u32)((pos.x + TILE_WIDTH_METERS * 0.5f) / TILE_WIDTH_METERS);
        u32 y = (u32)((pos.y + TILE_HEIGHT_METERS * 0.5f) / TILE_HEIGHT_METERS);
        if (x >= 0 && x < m_tilesPerWidth &&
            y >= 0 && y < m_tilesPerHeight)
        {
            return m_tiles + x + (y * m_tilesPerWidth);
        }

        return nullptr;
    }

    b32 Search(NavTile *start, NavTile *end, Path *out);

    void ReturnPathToPool(Path *path);

    static f32 TILE_WIDTH_METERS;
    static f32 TILE_HEIGHT_METERS;
    u32 m_tilesPerWidth;
    u32 m_tilesPerHeight;

private:
    NavTile *m_tiles;
    MemoryPool<PathTile> m_pathTilePool;
};

f32 NavMesh::TILE_WIDTH_METERS = 0.5f;
f32 NavMesh::TILE_HEIGHT_METERS = 0.5f;

struct Camera
{
    v2f m_pos;
    v2f m_dPos;
};

enum ENTITY_TYPE
{
    PLAYER,
    MONSTER,
    WALL,
};

struct Entity
{
    Entity() = default;
    ~Entity()
    {
        // TODO: Handle it!
        if (m_pathFollower)
        {
            delete m_pathFollower;
            m_pathFollower = nullptr;
        }
    }
    ENTITY_TYPE m_type;
    v2f m_pos;
    v2f m_dPos;
    Rectangle2 m_rect;
    v4f m_color;

    NavTile *m_activeTile;
    Waypoint *m_nextWaypoint;
    PathFollower *m_pathFollower;
};

class App : public AGE
{
public:
    App() : AGE("AGEApp")
    {
    }

    ~App(){};

    void Start() override;
    void Update(const f32 deltaTime) override;
    Camera m_camera;

private:
    Entity *CreatePlayerEntity(v2f pos, v4f color);
    Entity *CreateMonsterEntity(v2f pos, v4f color);
    Entity *CreateWallEntity(v2f pos, v4f color);

    void RenderCursor(RenderGroup *rg, v2f mousePos);

    void FreeEntity(Entity *entity);

    MemoryStack *m_gameSessionStack;
    NavMesh m_navMesh;
    MemoryPool<Entity> m_entityPool;
    Entity *m_firstEntity;
    Entity *m_monster;
    Waypoint *m_firstWaypoint;
    v2f m_monsterStart;
    u32 m_monsterCount;
};

Entity *gSelectedEntity;
NavTile *gPathEnd;
NavTileState gPaintState = EMPTY;

static b32 EntityStartPath(NavMesh *navMesh, Entity *entity, NavTile *end)
{
    if (navMesh->Search(entity->m_activeTile, end, &entity->m_pathFollower->m_path))
    {
        entity->m_pathFollower->m_targetTile = entity->m_pathFollower->m_path.m_start->m_next;
        entity->m_pathFollower->m_targetPos = entity->m_pathFollower->m_targetTile->m_tile->m_pos;
        return true;
    }
    return false;
}

static RCDrawQuad *PushTile(RenderGroup *rg, NavTile *tile)
{
    RCDrawQuad *drawTileCmd = rg->PushCommand<RCDrawQuad>();
    switch (tile->m_state)
    {
    case BLOCKING:
    {
        drawTileCmd->color = TILE_OCCUPIED_COLOR;
    }
    break;
    case PATH:
    {
        drawTileCmd->color = TILE_PATH_COLOR;
    }
    break;
    case PATH_END:
    {
        drawTileCmd->color = TILE_PATH_END_COLOR;
    }
    break;
    case PATH_TRAVERSED:
    {
        drawTileCmd->color = TILE_PATH_TRAVERSED_COLOR;
    }
    break;
    default:
        drawTileCmd->color = TILE_EMPTY_COLOR;
    }

    drawTileCmd->pos = v2f{tile->m_pos.x, tile->m_pos.y};
    drawTileCmd->dim = v2f{tile->m_rect.dim.x, tile->m_rect.dim.y};
    return drawTileCmd;
}

void App::RenderCursor(RenderGroup *rg, v2f mousePos)
{
    NavTile *tile = m_navMesh.GetTileAt(mousePos);
    if (tile == nullptr)
        return;

    RCDrawQuad *drawTileCmd = PushTile(rg, tile);
    if (tile->m_rect.Inside(mousePos, tile->m_pos))
    {
        if (Input::Binding->IsKeyDown(KEY::M1))
        {
            if (gPaintState == PATH_END)
            {
                gPathEnd = tile;
            }
            else if (gPaintState == BLOCKING)
            {
                CreateWallEntity(tile->m_pos, COLORS::YELLOW);
            }
            tile->m_state = gPaintState;
        }

        switch (gPaintState)
        {
        case BLOCKING:
        {
            drawTileCmd->color = TILE_OCCUPIED_COLOR;
        }
        break;
        case PATH:
        {
            drawTileCmd->color = TILE_PATH_COLOR;
        }
        break;
        case PATH_END:
        {
            drawTileCmd->color = TILE_PATH_END_COLOR;
        }
        break;
        default:
            drawTileCmd->color = TILE_EMPTY_COLOR;
        }
    }

    RCDrawQuad *mouseQuad = rg->PushCommand<RCDrawQuad>();
    mouseQuad->pos = mousePos;
    mouseQuad->color = COLORS::WHITE;
    mouseQuad->dim = v2f{.1f, .1f};
}

void Path::PushPath(RenderGroup *rg)
{
    PathTile *currentTile = m_start;
    while (currentTile != m_end)
    {
        RCDrawLine *drawLineCmd = rg->PushCommand<RCDrawLine>();
        drawLineCmd->start = currentTile->m_tile->m_pos;
        drawLineCmd->end = currentTile->m_next->m_tile->m_pos;
        drawLineCmd->colorStart = COLORS::RED;
        drawLineCmd->colorEnd = COLORS::GREEN;
        currentTile = currentTile->m_next;
    }
}

void NavMesh::Init(MemoryStack *allocator, u32 tilesPerWidth, u32 tilesPerHeight)
{
    m_tiles = allocator->AllocateAndConstruct<NavTile>(tilesPerWidth * tilesPerHeight);
    m_pathTilePool.PartitionFrom(allocator, 1000);
    m_pathTilePool.Init();
    m_tilesPerWidth = tilesPerWidth;
    m_tilesPerHeight = tilesPerHeight;
    for (s32 y = 0; y < (s32)m_tilesPerHeight; ++y)
    {
        for (s32 x = 0; x < (s32)m_tilesPerWidth; ++x)
        {
            NavTile *tile = &m_tiles[y * m_tilesPerWidth + x];
            *tile = {};
            tile->m_color = TILE_EMPTY_COLOR;
            tile->m_pos = v2f{(f32)(x * NavMesh::TILE_WIDTH_METERS),
                              (f32)(y * NavMesh::TILE_HEIGHT_METERS)};
            tile->m_rect = {};
            tile->m_rect.dim = v2f{NavMesh::TILE_WIDTH_METERS,
                                     NavMesh::TILE_HEIGHT_METERS};
            tile->m_state = EMPTY;

            Link *linkIter = &tile->m_links[0];
            NavTile *neighbour = nullptr;
            int iterCount = 0;
            for (s32 yy = y - 1; yy <= y + 1; ++yy)
            {
                for (s32 xx = x - 1; xx <= x + 1; ++xx)
                {
                    ++iterCount;
                    if (yy == y && xx == x)
                        continue;

                    if (yy < 0 || yy >= (s32)m_tilesPerHeight ||
                        xx < 0 || xx >= (s32)m_tilesPerWidth)
                    {
                        neighbour = nullptr;
                    }
                    else
                    {
                        neighbour = &m_tiles[yy * m_tilesPerWidth + xx];
                    }

                    linkIter->m_cost = 1.0f;
                    if ((iterCount % 2) == 1)
                        linkIter->m_cost += 0.1f;

                    linkIter->m_target = neighbour;
                    ++linkIter;
                }
            }
        }
    }
}

void NavMesh::Render(App *app, RenderGroup *rg)
{
    // NOTE(pf): Paint NavTiles...
    v2f mousePos = Input::Binding->Axis(AXIS::MOUSE);
    mousePos = (1.f / rg->m_metersToPixels) * mousePos;
    mousePos += app->m_camera.m_pos;
    for (u32 i = 0; i < m_tilesPerWidth * m_tilesPerHeight; ++i)
    {
        NavTile *tile = &m_tiles[i];
        RCDrawQuad *drawTileCmd = PushTile(rg, tile);
        if (tile->m_rect.Inside(mousePos, tile->m_pos))
        {
            if (Input::Binding->IsKeyHeld(KEY::M1))
            {
                if (gPaintState == PATH_END)
                {
                    gPathEnd = tile;
                }
                tile->m_state = gPaintState;
            }

            switch (gPaintState)
            {
            case BLOCKING:
            {
                drawTileCmd->color = TILE_OCCUPIED_COLOR;
            }
            break;
            case PATH:
            {
                drawTileCmd->color = TILE_PATH_COLOR;
            }
            break;
            case PATH_END:
            {
                drawTileCmd->color = TILE_PATH_END_COLOR;
            }
            break;
            default:
                drawTileCmd->color = TILE_EMPTY_COLOR;
            }
        }
    }
    for (u32 i = 0; i < m_tilesPerWidth * m_tilesPerHeight; ++i)
    {
        NavTile *tile = &m_tiles[i];
        for (int j = 0; j < NEIGHBOUR_COUNT; ++j)
        {
            Link *link = &tile->m_links[j];
            if (link->m_target == nullptr)
                continue;

            auto dl = rg->PushCommand<RCDrawLine>();
            dl->start = tile->m_pos;
            dl->end = link->m_target->m_pos;
            dl->colorStart = COLORS::BLUE;
            dl->colorEnd = COLORS::RED;
        }
    }
}

void NavMesh::Clear()
{

    for (u32 i = 0; i < m_tilesPerWidth * m_tilesPerHeight; ++i)
        if (m_tiles[i].m_state != BLOCKING)
            m_tiles[i].m_state = EMPTY;
}

b32 NavMesh::Search(NavTile *start, NavTile *end, Path *out)
{
    if (start == nullptr || end == nullptr || start == end ||
        start->m_state == BLOCKING || end->m_state == BLOCKING)
        return false;

    for (u32 i = 0; i < m_tilesPerWidth * m_tilesPerHeight; ++i)
        m_tiles[i].Reset();

    typedef std::pair<f32, NavTile *> PQEle;
    auto heuristic = [](NavTile *src, NavTile *dst)
    {
        return (dst->m_pos - src->m_pos).length2();
    };

    std::priority_queue<PQEle, std::vector<PQEle>, std::greater<PQEle>> openSet;
    openSet.push(std::make_pair(0.0f, start));

    start->m_activeCost = 0.0f;

    while (!openSet.empty())
    {
        NavTile *currentNavTile = openSet.top().second;
        if (currentNavTile == end)
        {
            u32 preEle = m_pathTilePool.ElementsInUse();
            out->m_end = m_pathTilePool.AllocateAndInit();
            out->m_end->m_tile = end;
            out->m_start = out->m_end;
            out->m_length = 1;
            while (out->m_start->m_tile != start)
            {
                PathTile *next = m_pathTilePool.AllocateAndInit();
                next->m_next = out->m_start;
                next->m_tile = out->m_start->m_tile->m_parent;
                out->m_start = next;
                ++out->m_length;
            }
            Assert(out->m_length == m_pathTilePool.ElementsInUse() - preEle);
            return true;
        }

        openSet.pop();

        for (u32 i = 0; i < NEIGHBOUR_COUNT; ++i)
        {
            Link *link = &currentNavTile->m_links[i];
            if (link->m_target == nullptr || link->m_target->m_state == BLOCKING)
            {
                continue;
            }

            // NOTE(pf): Heuristic towards goal.
            f32 score = currentNavTile->m_activeCost + link->m_cost;
            f32 targetScore = link->m_target->m_activeCost + link->m_cost;
            if (score < targetScore)
            {
                link->m_target->m_parent = currentNavTile;
                link->m_target->m_activeCost = score;
                openSet.push(std::make_pair(score + heuristic(link->m_target, end), link->m_target));
            }
        }
    }

    return false;
}

void NavMesh::ReturnPathToPool(Path *path)
{
    while (path->m_start != path->m_end)
    {
        m_pathTilePool.Free(path->m_start);
        path->m_start = path->m_start->m_next;
    }
    m_pathTilePool.Free(path->m_end);
    path->m_start = nullptr;
    path->m_end = nullptr;
    path->m_length = 0;
}

Entity *App::CreatePlayerEntity(v2f pos, v4f color)
{
    Entity *result = m_entityPool.AllocateAndInit();
    result->m_type = PLAYER;
    result->m_pos = pos;
    result->m_color = color;
    result->m_rect.dim = v2f{NavMesh::TILE_WIDTH_METERS, NavMesh::TILE_HEIGHT_METERS};
    result->m_activeTile = m_navMesh.GetTileAt(result->m_pos);
    // result->m_activeTile->m_state = BLOCKING;
    result->m_pathFollower = new PathFollower(); // TODO: Handle it!
    result->m_pathFollower->m_targetPos = result->m_pos;
    ((MemoryPool<Entity>::MemPtr *)result)->m_next = ((MemoryPool<Entity>::MemPtr *)m_firstEntity);
    m_firstEntity = result;
    return result;
}

Entity *App::CreateMonsterEntity(v2f pos, v4f color)
{
    Entity *result = m_entityPool.AllocateAndInit();
    result->m_type = MONSTER;
    result->m_pos = pos;
    result->m_color = color;
    result->m_rect.dim = v2f{NavMesh::TILE_WIDTH_METERS, NavMesh::TILE_HEIGHT_METERS};
    result->m_activeTile = m_navMesh.GetTileAt(result->m_pos);
    // result->m_activeTile->m_state = BLOCKING;
    result->m_pathFollower = new PathFollower(); // TODO: Handle it!
    result->m_pathFollower->m_targetPos = result->m_pos;
    ((MemoryPool<Entity>::MemPtr *)result)->m_next = ((MemoryPool<Entity>::MemPtr *)m_firstEntity);
    m_firstEntity = result;
    ++m_monsterCount;
    return result;
}

Entity *App::CreateWallEntity(v2f pos, v4f color)
{
    Entity *result = m_entityPool.AllocateAndInit();
    result->m_type = WALL;
    result->m_pos = pos;
    result->m_color = color;
    result->m_rect.dim = v2f{NavMesh::TILE_WIDTH_METERS, NavMesh::TILE_HEIGHT_METERS};
    result->m_activeTile = m_navMesh.GetTileAt(result->m_pos);
    result->m_activeTile->m_state = BLOCKING;
    ((MemoryPool<Entity>::MemPtr *)result)->m_next = ((MemoryPool<Entity>::MemPtr *)m_firstEntity);
    m_firstEntity = result;
    return result;
}

void App::FreeEntity(Entity *entity)
{
    if (m_firstEntity == entity)
    {
        m_firstEntity = &((MemoryPool<Entity>::MemPtr *)(entity))->m_next->m_element;
    }
    else
    {
        MemoryPool<Entity>::MemPtr *preEntity = (MemoryPool<Entity>::MemPtr *)m_firstEntity;
        while (preEntity->m_next)
        {
            if (&(preEntity->m_next->m_element) == entity)
            {
                preEntity->m_next = ((MemoryPool<Entity>::MemPtr *)entity)->m_next;
                break;
            }
            preEntity = preEntity->m_next;
        }
    }
    m_entityPool.FreeAndDeconstruct(entity);
}

void App::Start()
{
    m_monsterCount = 0;
    m_monsterStart = v2f{2.0f, 2.0f};

    m_gameSessionStack = m_permMemStack.Partition(MB(2));
    m_entityPool.PartitionFrom(m_gameSessionStack, 256);
    m_entityPool.Init();
    m_firstEntity = nullptr;
    m_navMesh.Init(m_gameSessionStack, MAP_WIDTH, MAP_HEIGHT);

    v4f wallColor = TILE_OCCUPIED_COLOR;
    s32 halfWayX = (s32)(m_navMesh.m_tilesPerWidth * 0.5f);
    s32 halfWayY = (s32)(m_navMesh.m_tilesPerHeight * 0.5f);
    Waypoint *wps = m_frameMemStack.AllocateAndInit<Waypoint>(9);
    for (s32 y = 0; y < (s32)m_navMesh.m_tilesPerHeight; ++y)
    {
        for (s32 x = 0; x < (s32)m_navMesh.m_tilesPerWidth; ++x)
        {
            char type = MAP_ONE[(MAP_HEIGHT - 1) - y][x];
            if (type == 'W')
            {
                v2f wallPos = m_navMesh.GetTileAt(v2i{x, y})->m_pos;
                CreateWallEntity(wallPos, wallColor);
            }
            else if (type == 'S')
            {
                m_monsterStart = m_navMesh.GetTileAt(v2i{x, y})->m_pos;
            }
            else if ((type - '1') >= 0 && (type - '9') <= 0)
            {
                (wps + (type - '1'))->m_target = m_navMesh.GetTileAt(v2i{x, y});
                (wps + (type - '1'))->m_next = (wps + (type - '1'));
            }
        }
    }

    m_firstWaypoint = m_gameSessionStack->AllocateAndClone<Waypoint>(wps);
    Assert(m_firstWaypoint->m_next != nullptr);
    Waypoint *nextWP = m_firstWaypoint;
    for (int i = 1; i < 9; ++i)
    {
        Waypoint *wp = wps + i;
        if (wp->m_next == nullptr)
            continue;

        nextWP->m_next = m_gameSessionStack->AllocateAndClone<Waypoint>(wp);
        nextWP = nextWP->m_next;
        nextWP->m_next = nullptr;
    }

    CreatePlayerEntity(v2f{4.0f, 2.0f},
                       v4f{RandomF32(0.1, 1.0f), RandomF32(0.1f, 1.0f), RandomF32(0.25, .5f), 1});

    m_camera = {};

    m_camera.m_pos = v2f{-NavMesh::TILE_WIDTH_METERS * 6.f,
                         -NavMesh::TILE_HEIGHT_METERS * 3.f};
}

void App::Update(const f32 deltaTime)
{
    v2f cameraDPosDt = {};
    if (Input::Binding->IsKeyHeld(KEY::ARROW_LEFT))
    {
        cameraDPosDt.x -= 1.0f;
    }
    if (Input::Binding->IsKeyHeld(KEY::ARROW_UP))
    {
        cameraDPosDt.y += 1.0f;
    }
    if (Input::Binding->IsKeyHeld(KEY::ARROW_RIGHT))
    {
        cameraDPosDt.x += 1.0f;
    }
    if (Input::Binding->IsKeyHeld(KEY::ARROW_DOWN))
    {
        cameraDPosDt.y -= 1.0f;
    }

    const f32 CAMERA_SPEED = 5.0f;
    if (cameraDPosDt.length2() > 0.0f)
    {
        m_camera.m_dPos = cameraDPosDt.normalize() * CAMERA_SPEED;
    }
    else
    {
        m_camera.m_dPos = v2f{0.0f, 0.0f};
    }

    m_camera.m_pos += m_camera.m_dPos * deltaTime;

    if (Input::Binding->IsKeyDown(KEY::ESCAPE))
    {
        m_isAppRunning = false;
    }
    else if (Input::Binding->IsKeyDown(KEY::V))
    {
        Renderer::Binding->ToggleVSync();
    }
    else if (Input::Binding->IsKeyDown(KEY::F))
    {
        Renderer::Binding->ToggleFullscreen();
    }
    else if (Input::Binding->IsKeyDown(KEY::S))
    {
        if (gSelectedEntity != nullptr && gSelectedEntity->m_pathFollower != nullptr)
        {
            if (gSelectedEntity->m_pathFollower->m_path.IsValid())
            {
                m_navMesh.ReturnPathToPool(&gSelectedEntity->m_pathFollower->m_path);
            }

            EntityStartPath(&m_navMesh, gSelectedEntity, gPathEnd);
        }
    }
    else if (Input::Binding->IsKeyDown(KEY::C))
    {
        m_navMesh.Clear();
        gSelectedEntity = nullptr;
        gPathEnd = nullptr;
    }

    // LOGIC & RENDER:

    RenderGroup *renderGroup = Renderer::Binding->PushRenderGroup();
    renderGroup->m_offset = m_camera.m_pos;

    v2f mousePos = Input::Binding->Axis(AXIS::MOUSE);
    mousePos = (1.f / renderGroup->m_metersToPixels) * mousePos;
    mousePos += m_camera.m_pos;

    if (Input::Binding->IsKeyDown(KEY::K1))
    {
        gPaintState = BLOCKING;
    }
    else if (Input::Binding->IsKeyDown(KEY::K2))
    {
        gPaintState = EMPTY;
    }
    else if (Input::Binding->IsKeyDown(KEY::K3))
    {
        gPaintState = PATH_END;
    }

    RCClearColor *clearScreenCmd = renderGroup->PushCommand<RCClearColor>();
    clearScreenCmd->color = COLORS::BLACK;

    // m_navMesh.Render(this, renderGroup);

    // const float sAngle = PI * 0.0f;//deltaTime * 2.0f;
    const float sAngle = deltaTime * 2.0f;
    static v2f oldTarget = v2f{1.0f, 0.0f};

    RCDrawLine *dl = renderGroup->PushCommand<RCDrawLine>();
    dl->start = v2f{4.0f, 4.0f};
    dl->end.x = oldTarget.x * cos(sAngle) - oldTarget.y * sin(sAngle);
    dl->end.y = oldTarget.x * sin(sAngle) + oldTarget.y * cos(sAngle);
    oldTarget = dl->end;
    dl->end += dl->start;
    dl->colorStart = COLORS::RED;
    dl->colorEnd = COLORS::GREEN;

    // NOTE(pf): Grid pattern
    for (u32 y = 0; y < m_navMesh.m_tilesPerHeight; ++y)
    {
        RCDrawLine *gridLine = renderGroup->PushCommand<RCDrawLine>();
        gridLine->start = v2f{0.0f,
                                y * NavMesh::TILE_WIDTH_METERS};
        gridLine->end = v2f{(m_navMesh.m_tilesPerWidth - 1) * NavMesh::TILE_WIDTH_METERS,
                              y * NavMesh::TILE_WIDTH_METERS};
        gridLine->colorStart = COLORS::WHITE;
        gridLine->colorEnd = COLORS::WHITE;
    }

    for (u32 x = 0; x < m_navMesh.m_tilesPerWidth; ++x)
    {
        RCDrawLine *gridLine = renderGroup->PushCommand<RCDrawLine>();
        gridLine->start = v2f{x * NavMesh::TILE_WIDTH_METERS,
                                0.0f};
        gridLine->end = v2f{x * NavMesh::TILE_WIDTH_METERS,
                              (m_navMesh.m_tilesPerHeight - 1) * NavMesh::TILE_WIDTH_METERS};
        gridLine->colorStart = COLORS::WHITE;
        gridLine->colorEnd = COLORS::WHITE;
    }

    // TODO(pf): Some better way of handling this iteration, based of the construction of the memory
    for (MemoryPool<Entity>::MemPtr *it = (MemoryPool<Entity>::MemPtr *)m_firstEntity;;)
    {
        Entity *activeEntity = &it->m_element;
        const f32 ENT_SPEED = 2.0f;
        const f32 ARRIVAL_DEADZONE = 0.001f;
        // TODO: Fancy movements.
        PathFollower *pathFollower = activeEntity->m_pathFollower;
        if (pathFollower != nullptr)
        {
            if (pathFollower->m_path.IsValid())
            {
                pathFollower->m_path.PushPath(renderGroup);
            }

            if ((pathFollower->m_targetPos - activeEntity->m_pos).length() < ARRIVAL_DEADZONE)
            {
                activeEntity->m_pos = pathFollower->m_targetPos;
            }
            else
            {
                v2f absDist = pathFollower->m_targetPos - activeEntity->m_pos;
                f32 lengthBetween = absDist.length();
                activeEntity->m_dPos = absDist.normalize();

                v2f deltaPos = activeEntity->m_dPos * deltaTime * ENT_SPEED;
                if (deltaPos.length() > lengthBetween)
                {
                    activeEntity->m_pos = pathFollower->m_targetPos;
                    activeEntity->m_dPos = v2f{0.0f, 0.0f};
                }
                else
                {
                    activeEntity->m_pos += deltaPos;
                }
            }

            NavTile *tile = m_navMesh.GetTileAt(activeEntity->m_pos);
            if (tile != activeEntity->m_activeTile)
            {
                // activeEntity->m_activeTile->m_state = EMPTY;
                activeEntity->m_activeTile = tile;
                // activeEntity->m_activeTile->m_state = BLOCKING;
            }

            if (pathFollower->m_targetTile != nullptr &&
                activeEntity->m_activeTile == pathFollower->m_targetTile->m_tile)
            {
                if (pathFollower->m_path.m_end == pathFollower->m_targetTile->m_next)
                {
                    pathFollower->m_targetPos = pathFollower->m_targetTile->m_next->m_tile->m_pos;
                    m_navMesh.ReturnPathToPool(&pathFollower->m_path);
                    pathFollower->m_targetTile = nullptr;
                }
                else
                {
                    pathFollower->m_targetTile = pathFollower->m_targetTile->m_next;
                    pathFollower->m_targetPos = pathFollower->m_targetTile->m_tile->m_pos;
                }
            }
        }

        if (Input::Binding->IsKeyDown(KEY::M1))
        {
            if (activeEntity->m_rect.Inside(mousePos, activeEntity->m_pos))
            {
                gSelectedEntity = activeEntity;
            }
        }

        // TODO(pf): Center entity position.
        if (activeEntity->m_type == PLAYER)
        {
            // TODO: Make into a draw call to make an outline.
            RCDrawQuad *highlightCmd = renderGroup->PushCommand<RCDrawQuad>();
            highlightCmd->color = gSelectedEntity == activeEntity ? COLORS::GREEN : COLORS::RED;
            highlightCmd->pos = activeEntity->m_pos;
            highlightCmd->dim = activeEntity->m_rect.dim + v2f{0.05f, 0.05f};
        }
        else if (activeEntity->m_type == MONSTER)
        {
            // No target tile ?..
            if (activeEntity->m_pathFollower->m_targetTile == nullptr)
            {
                // .. no waypoint yet ? get one!
                if (activeEntity->m_nextWaypoint == nullptr)
                {
                    activeEntity->m_nextWaypoint = m_firstWaypoint;
                    EntityStartPath(&m_navMesh, activeEntity, activeEntity->m_nextWaypoint->m_target);
                }
                // .. otherwise, reached waypoint ?
                else if (activeEntity->m_activeTile == activeEntity->m_nextWaypoint->m_target)
                {
                    // .. which is last ? adios!
                    if (activeEntity->m_nextWaypoint->m_next == nullptr)
                    {
                        // TODO: Maybe not free/alloc inside main loop.. currently need to patch up iterator.
                        if (it->m_next == nullptr)
                        {
                            FreeEntity(activeEntity);
                            --m_monsterCount;
                            break;
                        }
                        else
                        {
                            Entity *entityToFree = activeEntity;
                            it = it->m_next;
                            FreeEntity(entityToFree);
                            --m_monsterCount;
                        }
                    }
                    // .. otherwise get next in line.
                    else
                    {
                        activeEntity->m_nextWaypoint = activeEntity->m_nextWaypoint->m_next;
                        EntityStartPath(&m_navMesh, activeEntity, activeEntity->m_nextWaypoint->m_target);
                    }
                }
            }
        }

        RCDrawQuad *entityRndCmd = renderGroup->PushCommand<RCDrawQuad>();
        entityRndCmd->color = activeEntity->m_color;
        entityRndCmd->pos = activeEntity->m_pos;
        entityRndCmd->dim = activeEntity->m_rect.dim;

        if (it->m_next == nullptr)
            break;
        it = it->m_next;
    }

#define MAX_MONSTER_COUNT 0
    for (int i = m_monsterCount; i < MAX_MONSTER_COUNT; ++i)
    {
        CreateMonsterEntity(m_monsterStart, COLORS::RED);
    }

    RenderCursor(renderGroup, mousePos);
}

struct GameCode
{
    HMODULE gameCodeDLL;
    GameUpdate_t *gameUpdate;
};

GameCode LoadGameCode()
{
    GameCode result = {};
    CopyFileA("game.dll", "game_temp.dll", FALSE);
    result.gameCodeDLL = LoadLibraryA("game_temp.dll");
    if (result.gameCodeDLL)
    {
        result.gameUpdate = (GameUpdate_t *)GetProcAddress(result.gameCodeDLL, "GameUpdate");
    }
    else
    {
        // TODO(pf): Logging.
    }
    return result;
}

void UnloadGameCode(GameCode *gc)
{
    if (gc->gameCodeDLL)
    {
        FreeLibrary(gc->gameCodeDLL);
        gc->gameCodeDLL = 0;
    }

    gc->gameUpdate = GameUpdateStub;
}

// int main(int argc, char** argv)
int main()
{

    App app;
    GameCode gc = LoadGameCode();
    // TODO: Handle OS specifics here instead of in the app.
    while (1)
    {
        if (gc.gameUpdate)
        {
            gc.gameUpdate(nullptr, nullptr, nullptr);
        }
        else
        {
            break;
        }
    }
    // //app.HandleConsoleInput(argc, argv);
    // app.Init(960, 540);
    // //app.Init(1920, 1080);
    // //app.Init(1280, 720);
    // app.Run();
    return 0;
}

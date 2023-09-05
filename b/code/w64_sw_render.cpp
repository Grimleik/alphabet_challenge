/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#include "w64_render.h"
#include "w64_sw_render.h"

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

RendererWinSoft::RendererWinSoft() : Renderer()
{
}

RendererWinSoft::~RendererWinSoft() {}

void RendererWinSoft::Init(void *platformHandle)
{
    m_w32State = (W32State *)platformHandle;
    m_wpPrev = {sizeof(m_wpPrev)};
    m_backBuffer.m_memory = VirtualAlloc(0,
                                         m_backBuffer.m_width * m_backBuffer.m_height * m_backBuffer.m_bytesPerPixel,
                                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    m_backBuffer.m_width = m_w32State->windowWidth;
    m_backBuffer.m_height = m_w32State->windowHeight;
    m_backBuffer.m_bytesPerPixel = 4;
    m_backBuffer.m_pitch = m_backBuffer.m_width * m_backBuffer.m_bytesPerPixel;

    m_backBufferInfo.bmiHeader.biSize = sizeof(m_backBufferInfo.bmiHeader);
    m_backBufferInfo.bmiHeader.biWidth = m_backBuffer.m_width;
    m_backBufferInfo.bmiHeader.biHeight = -(s32)(m_backBuffer.m_height);
    m_backBufferInfo.bmiHeader.biPlanes = 1;
    m_backBufferInfo.bmiHeader.biBitCount = (WORD)(m_backBuffer.m_bytesPerPixel * 8);
    m_backBufferInfo.bmiHeader.biCompression = BI_RGB;

    m_deviceContext = GetDC(m_w32State->hwnd);

    backBufferHalfWidth = m_backBuffer.m_width * 0.5f;
    backBufferHalfHeight = m_backBuffer.m_height * 0.5f;
}

void RendererWinSoft::ToggleFullscreen()
{
    HWND hwnd = m_w32State->hwnd;
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(hwnd, &m_wpPrev) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            SetWindowLong(hwnd, GWL_STYLE,
                          style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &m_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void RendererWinSoft::ConvertPositions(RenderGroup *rg, f32 *x, f32 *y)
{
    switch (rg->m_type)
    {
    case RENDERGROUP_TYPE::ORTHOGRAPHIC:
    {
        // TODO: Sub pixel movement..
        *x = ((*x - rg->m_offset.x) * rg->m_metersToPixels);
        *y = ((*y - rg->m_offset.y) * rg->m_metersToPixels);
        // NOTE(pf): y flip.
        *y = m_w32State->windowHeight - *y;
    }
    break;

    case RENDERGROUP_TYPE::UI:
    {
        // TODO: Sub pixel movement..
        *x = (*x * backBufferHalfWidth) + backBufferHalfWidth;
        *y = (*y * backBufferHalfHeight) + backBufferHalfHeight;
        // NOTE(pf): y flip.
        *y = m_w32State->windowHeight - *y;
    }
    break;

    default:
        break;
    }
}

void RendererWinSoft::ConvertPositionsAndDimensions(RenderGroup *rg, f32 *x, f32 *y, f32 *w, f32 *h)
{
    switch (rg->m_type)
    {
    case RENDERGROUP_TYPE::ORTHOGRAPHIC:
    {
        // TODO: Sub pixel movement..
        *x = ((*x - rg->m_offset.x) * rg->m_metersToPixels);
        *y = ((*y - rg->m_offset.y) * rg->m_metersToPixels);
        // NOTE(pf): y flip.
        *y = m_w32State->windowHeight - *y;
        *w = *w * rg->m_metersToPixels;
        *h = *h * rg->m_metersToPixels;
    }
    break;

    case RENDERGROUP_TYPE::UI:
    {
        // TODO: Sub pixel movement..
        *x = (*x * backBufferHalfWidth) + backBufferHalfWidth;
        *y = (*y * backBufferHalfHeight) + backBufferHalfHeight;
        // NOTE(pf): y flip.
        *y = m_w32State->windowHeight - *y;
        *w = *w * backBufferHalfWidth;
        *h = *h * m_backBuffer.m_height;
    }
    break;

    default:
        break;
    }
}

void RendererWinSoft::ExecuteCommandList(RenderList *list)
{
    u8 *baseAddr = (u8 *)list->renderGroups.m_memory;
    for (
        ; baseAddr != list->renderGroups.EndIterator();) // ++rg)
    {
        RenderGroup *rg = (RenderGroup *)baseAddr;
        baseAddr = (u8 *)rg->m_commandStack->m_memory;
        for (; baseAddr != rg->m_commandStack->EndIterator();)
        {
            RenderCommand *rc = (RenderCommand *)baseAddr;
            switch (rc->type)
            {
            case RC_TYPE::RC_ClearColor:
            {
                RCClearColor *cmd = (RCClearColor *)rc;
                m_backBuffer.Clear(cmd->color);
                baseAddr += sizeof(RCClearColor);
            }
            break;
            case RC_TYPE::RC_DrawQuad:
            {
                RCDrawQuad *cmd = (RCDrawQuad *)rc;
                f32 x = cmd->pos.x, y = cmd->pos.y, w = cmd->dim.x, h = cmd->dim.y;
                ConvertPositionsAndDimensions(rg, &x, &y, &w, &h);
                m_backBuffer.DrawQuad((s32)x, (s32)y, (s32)w, (s32)h, cmd->color);
                baseAddr += sizeof(RCDrawQuad);
            }
            break;
            case RC_TYPE::RC_DrawLine:
            {
                RCDrawLine *cmd = (RCDrawLine *)rc;
                cmd->start -= rg->m_offset;
                cmd->end -= rg->m_offset;
                // TODO: Sub pixel movement..
                s32 x0 = (s32)(cmd->start.x * rg->m_metersToPixels);
                s32 y0 = (s32)(cmd->start.y * rg->m_metersToPixels);

                y0 = m_w32State->windowHeight - y0;

                s32 x1 = (s32)(cmd->end.x * rg->m_metersToPixels);
                s32 y1 = (s32)(cmd->end.y * rg->m_metersToPixels);
                y1 = m_w32State->windowHeight - y1;

                m_backBuffer.DrawLine(x0, y0, x1, y1, cmd->colorStart, cmd->colorEnd);
                baseAddr += sizeof(RCDrawLine);
            }
            break;
            case RC_TYPE::RC_DrawCircle:
            {
                RCDrawCircle *cmd = (RCDrawCircle *)rc;
                cmd->center -= rg->m_offset;
                // TODO: Sub pixel movement..
                s32 centerX = (s32)(cmd->center.x * rg->m_metersToPixels);
                s32 centerY = (s32)(cmd->center.y * rg->m_metersToPixels);
                // NOTE(pf): y flip.
                centerY = m_w32State->windowHeight - centerY;
                s32 w = (u32)(cmd->radius * rg->m_metersToPixels);
                s32 h = (u32)(cmd->radius * rg->m_metersToPixels);
                f32 radiusInPixels = cmd->radius * cmd->radius * rg->m_metersToPixels * rg->m_metersToPixels;

                // NOTE(pf): Horribly inefficient :D
                for (s32 y0 = Max<s32>(centerY - h, 0);
                     y0 <= Min<s32>(centerY + h, m_backBuffer.m_height);
                     ++y0)
                {
                    for (s32 x0 = Max<s32>(centerX - w, 0);
                         x0 <= Min<s32>(centerX + w, m_backBuffer.m_width);
                         ++x0)
                    {
                        v2f v = {(f32)x0 - centerX, (f32)y0 - centerY};
                        if (v.length2() <= radiusInPixels)
                        {
                            m_backBuffer.DrawPixel(x0, y0, cmd->color);
                        }
                    }
                }

                baseAddr += sizeof(RCDrawCircle);
            }
            break;
            case RC_TYPE::RC_DrawCircleOultine:
            {
                RCDrawCircleOutline *cmd = (RCDrawCircleOutline *)rc;
                cmd->center -= rg->m_offset;
                s32 centerX = (s32)(cmd->center.x * rg->m_metersToPixels);
                s32 centerY = (s32)(cmd->center.y * rg->m_metersToPixels);
                // NOTE(pf): y flip.
                centerY = m_w32State->windowHeight - centerY;
                s32 w = (u32)((cmd->radius + cmd->thickness) * rg->m_metersToPixels);
                s32 h = (u32)((cmd->radius + cmd->thickness) * rg->m_metersToPixels);

                f32 radiusInPixels = SquareF32(cmd->radius * rg->m_metersToPixels);
                f32 thicknessInPixels = SquareF32(cmd->thickness * rg->m_metersToPixels);
                // NOTE(pf): Horribly inefficient :D
                for (s32 y0 = Max<s32>(centerY - h, 0);
                     y0 <= Min<s32>(centerY + h, m_backBuffer.m_height);
                     ++y0)
                {
                    for (s32 x0 = Max<s32>(centerX - w, 0);
                         x0 <= Min<s32>(centerX + w, m_backBuffer.m_width);
                         ++x0)
                    {
                        v2f v = {(f32)x0 - centerX, (f32)y0 - centerY};
                        if (abs(v.length2() - radiusInPixels) <= thicknessInPixels)
                        {
                            m_backBuffer.DrawPixel(x0, y0, cmd->color);
                        }
                    }
                }

                baseAddr += sizeof(RCDrawCircleOutline);
            }
            case RC_TYPE::RC_DrawQuadOutline:
            {
                RCDrawQuadOutline *cmd = (RCDrawQuadOutline *)rc;
                cmd->pos -= rg->m_offset;
                // TODO: Sub pixel movement..
                s32 x = (s32)(cmd->pos.x * rg->m_metersToPixels);
                s32 y = (s32)(cmd->pos.y * rg->m_metersToPixels);
                // NOTE(pf): y flip.
                y = m_w32State->windowHeight - y;
                s32 w = (u32)(cmd->dim.x * rg->m_metersToPixels);
                s32 h = (u32)(cmd->dim.y * rg->m_metersToPixels);
                m_backBuffer.DrawQuadOutline(x, y, w, h,
                                             (s32)(cmd->thickness * rg->m_metersToPixels),
                                             cmd->color);
                baseAddr += sizeof(RCDrawQuadOutline);
            }
            break;
            case RC_TYPE::RC_DrawText:
            {
                RCDrawText *cmd = (RCDrawText *)rc;
                f32 x = cmd->pos.x, y = cmd->pos.y;
                ConvertPositions(rg, &x, &y);
                m_backBuffer.DrawTextCmd((s32)x, (s32)y, cmd->color, &cmd->txtInfo);
                baseAddr += sizeof(RCDrawText);
            }
            break;
            case RC_TYPE::RC_DrawTexture:
            {
                RCDrawTexture *cmd = (RCDrawTexture *)rc;
                f32 x = cmd->pos.x, y = cmd->pos.y;
                ConvertPositions(rg, &x, &y);
                m_backBuffer.DrawTexture((s32)x, (s32)y, cmd->color_tint, cmd->info);
                baseAddr += sizeof(RCDrawTexture);
            }
            break;
            default:
            {
                Assert(!"Should Never Come Here. Unsupported Draw Call Found");
            }
            break;
            }
        }
        baseAddr = (u8 *)rg->m_commandStack->m_memory + rg->m_commandStack->m_totalSize;
    }
}

void RendererWinSoft::FrameFlip()
{
    // NOTE: Test to use BitBlt for software rendering instead for performance reasons ?
    StretchDIBits(m_deviceContext,
                  0, 0,
                  m_backBuffer.m_width, m_backBuffer.m_height,
                  0, 0, m_backBuffer.m_width, m_backBuffer.m_height,
                  m_backBuffer.m_memory,
                  &m_backBufferInfo,
                  DIB_RGB_COLORS, SRCCOPY);

    if (settings.isVsync)
        Assert(DwmFlush() == S_OK);

    if (settings.shouldToggleFullscreen)
    {
        ToggleFullscreen();
        settings.shouldToggleFullscreen = false;
    }
}
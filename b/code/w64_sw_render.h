#if !defined(W64_SW_RENDER_H)
/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#define W64_SW_RENDER_H

#include "w32_state.h"
class RendererWinSoft : public Renderer
{
public:
    RendererWinSoft();
    ~RendererWinSoft();

    void Init(void *platformHandle) override;
    
protected:
    void ExecuteCommandList(RenderList *list) override;
    void FrameFlip() override;

    void ToggleFullscreen();

    void ConvertPositions(RenderGroup *rg, f32 *x, f32 *y);
    void ConvertPositionsAndDimensions(RenderGroup *rg, f32 *x, f32 *y, f32 *w, f32 *h);

    W32State *m_w32State;
    HDC m_deviceContext;
    WINDOWPLACEMENT m_wpPrev;
    DrawBuffer m_backBuffer;
    BITMAPINFO m_backBufferInfo;
    f32 backBufferHalfWidth;
    f32 backBufferHalfHeight;
};

#endif

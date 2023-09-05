#if !defined(W64_RENDER_H)
/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#define W64_RENDER_H

#include "age.h"

class Renderer
{
public:
    Renderer() : settings{} {}
    virtual ~Renderer(){}

    virtual void Init(void *platformHandle){}

    inline void Frame(RenderList *list);
    RenderSettings settings;
protected:
    virtual void ExecuteCommandList(RenderList *list) = 0;
    virtual void FrameFlip() = 0;
};

void Renderer::Frame(RenderList *list)
{
    ExecuteCommandList(list);
    FrameFlip();
}

#endif

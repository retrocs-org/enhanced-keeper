#include "pre_inc.h"
#include <cstring>
#include "keeperfx.hpp"
#include "kfx/renderer/RendererSoftware.h"
#include "bflib_video.h"       // PALETTE_COLORS, SDL, vsync_enabled
#include "kfx/platform/WindowSystemSDL.h"
#include "kfx/renderer/software/SwDisplaySurface.h"
#include "kfx/renderer/RendererManager.h" // RendererBeginFrame/EndFrame, around FGExecuteCursor
#include "kfx/renderer/RenderGraph.h"
#include "kfx/renderer/RenderTaskProducerRegistry.h"
#include "kfx/renderer/RendererFrameCounter.h"
#include "bflib_mouse.h"       // LbMouseOnBeginSwap/EndSwap (submits the cursor sprite around present)
#include <SDL3_image/SDL_image.h> // IMG_SavePNG (screenshots)
#include "post_inc.h"

bool RendererSoftware::Init()
{
    return true;
}

void RendererSoftware::Shutdown()
{
    destroy_present_target();
}

void RendererSoftware::SetDisplayPalette(const unsigned char* rgb8)
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    if (draw_surface == NULL)
        return;
    SDL_Color colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++)
    {
        colors[i].r = rgb8[3 * i + 0];
        colors[i].g = rgb8[3 * i + 1];
        colors[i].b = rgb8[3 * i + 2];
        colors[i].a = SDL_ALPHA_OPAQUE;
    }
    SDL_Palette* surfpal = SDL_GetSurfacePalette(draw_surface);
    if (surfpal != NULL)
        SDL_SetPaletteColors(surfpal, colors, 0, PALETTE_COLORS);
}

void RendererSoftware::ClearScreen(unsigned char colour)
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    if (draw_surface == NULL)
        return;
    if (!SDL_FillSurfaceRect(draw_surface, NULL, colour))
        ERRORLOG("Error while clearing screen: %s", SDL_GetError());
}

bool RendererSoftware::ensure_present_target()
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    SDL_Window* window = GetSDLWindowSystem()->GetSDLWindow();
    if (m_renderer != nullptr && SDL_GetRenderWindow(m_renderer) != window)
        destroy_present_target();
    if (m_renderer == nullptr)
    {
        m_renderer = SDL_CreateRenderer(window, NULL);
        if (m_renderer == nullptr)
        {
            ERRORLOG("SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }
        // Name the backend SDL picked for us, so a bug report tells which graphics path the
        // game was presenting through, and which driver libraries that pulls into the process.
        const char * backend = SDL_GetRendererName(m_renderer);
        SYNCLOG("Presenting through SDL renderer: %s", (backend != nullptr) ? backend : "unknown");
    }

    const int want_vsync = vsync_enabled ? 1 : 0;
    if (m_vsync != want_vsync)
    {
        SDL_SetRenderVSync(m_renderer, want_vsync);
        m_vsync = want_vsync;
    }

    if (m_texture == nullptr || m_tex_w != draw_surface->w || m_tex_h != draw_surface->h)
    {
        if (m_texture != nullptr) { SDL_DestroyTexture(m_texture); m_texture = nullptr; }
        m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STREAMING, draw_surface->w, draw_surface->h);
        if (m_texture == nullptr)
        {
            ERRORLOG("SDL_CreateTexture failed: %s", SDL_GetError());
            return false;
        }
        SDL_SetTextureScaleMode(m_texture, start_params.enhanced_mode ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        m_tex_w = draw_surface->w;
        m_tex_h = draw_surface->h;
    }
    return true;
}

void RendererSoftware::destroy_present_target()
{
    if (m_direct_texture != nullptr) { SDL_DestroyTexture(m_direct_texture); m_direct_texture = nullptr; }
    if (m_texture != nullptr) { SDL_DestroyTexture(m_texture); m_texture = nullptr; }
    if (m_renderer != nullptr) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    m_tex_w = 0;
    m_tex_h = 0;
    m_direct_tex_w = 0;
    m_direct_tex_h = 0;
    m_direct_alloc_w = 0;
    m_direct_alloc_h = 0;
    m_direct_filter = PRESENT_FILTER_NEAREST;
    m_direct_present = false;
    m_direct_rgba.clear();
    m_vsync = -1;
}

unsigned char* RendererSoftware::LockFramebuffer(int* out_pitch)
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    if (draw_surface == NULL || !SDL_LockSurface(draw_surface))
        return nullptr;
    if (out_pitch != nullptr)
        *out_pitch = draw_surface->pitch;
    return static_cast<unsigned char*>(draw_surface->pixels);
}

void RendererSoftware::UnlockFramebuffer()
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    if (draw_surface != NULL)
        SDL_UnlockSurface(draw_surface);
}

bool RendererSoftware::BeginFrame()
{
    return true;
}

void RendererSoftware::EndFrame()
{
}

bool RendererSoftware::ScheduleScreenshot(const char* path, int fmt)
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    if (draw_surface == NULL)
        return false;
    bool ok;
    switch (fmt)
    {
        case 1:  ok = IMG_SavePNG(draw_surface, path); break;
        case 2:  ok = SDL_SaveBMP(draw_surface, path); break;
        default: return false;
    }
    if (!ok)
        ERRORLOG("Screenshot save failed (%s): %s", path, SDL_GetError());
    return ok;
}

bool RendererSoftware::PresentImage(const struct RendererPresentImageDesc* desc)
{
    if (desc == nullptr || desc->src == nullptr ||
        (desc->kind != PRESENT_KIND_OPAQUE &&
         !(desc->kind == PRESENT_KIND_COMPOSITE && desc->format == PRESENT_FORMAT_RGBA8)) ||
        desc->src_w <= 0 || desc->src_h <= 0)
        return false;

    m_direct_rgba.resize((size_t)desc->src_w * (size_t)desc->src_h * 4u);

    if (desc->format == PRESENT_FORMAT_RGBA8)
    {
        const int pitch = (desc->src_pitch > 0) ? desc->src_pitch : desc->src_w * 4;
        const size_t row_bytes = (size_t)desc->src_w * 4u;
        for (int y = 0; y < desc->src_h; ++y)
            std::memcpy(m_direct_rgba.data() + (size_t)y * row_bytes,
                        desc->src + (size_t)y * (size_t)pitch, row_bytes);
    }
    else if (desc->format == PRESENT_FORMAT_INDEXED8 &&
             desc->palette == PRESENT_PALETTE_EMBEDDED &&
             desc->embedded_palette != nullptr)
    {
        const int pitch = (desc->src_pitch > 0) ? desc->src_pitch : desc->src_w;
        for (int y = 0; y < desc->src_h; ++y)
        {
            const unsigned char* src = desc->src + (size_t)y * (size_t)pitch;
            unsigned char* dst = m_direct_rgba.data() + (size_t)y * (size_t)desc->src_w * 4u;
            for (int x = 0; x < desc->src_w; ++x)
            {
                const unsigned char* pal = desc->embedded_palette + (size_t)src[x] * 4u;
                dst[x * 4 + 0] = pal[2];
                dst[x * 4 + 1] = pal[1];
                dst[x * 4 + 2] = pal[0];
                dst[x * 4 + 3] = 255;
            }
        }
    }
    else
    {
        return false;
    }

    m_direct_tex_w = desc->src_w;
    m_direct_tex_h = desc->src_h;
    m_direct_dst_x = desc->dst_x;
    m_direct_dst_y = desc->dst_y;
    m_direct_dst_w = desc->dst_w;
    m_direct_dst_h = desc->dst_h;
    m_direct_filter = desc->filter;
    m_direct_composite = desc->kind == PRESENT_KIND_COMPOSITE;
    m_direct_present = true;
    return true;
}

void RendererSoftware::PresentFrame()
{
    SDL_Surface* draw_surface = SwDisplaySurfaceGet();
    if (draw_surface == NULL || !ensure_present_target())
        return;

    // Same call as RendererOpenGL::PresentFrame(), placed ahead of
    // RenderGraph::Execute() below so producers append using this frame's
    // number before it advances.
    RenderTaskProducerRegistry_ProduceAll(RendererFrameCounter_Current());
    LbMouseOnBeginSwap();
    if (RendererBeginFrame())
    {
        RenderGraph::Execute(*this);
        RendererEndFrame();
    }
    RendererFrameCounter_Advance();

    int px = 0, py = 0, pw = 0, ph = 0;
    GetSDLWindowSystem()->GetPresentRect(&px, &py, &pw, &ph);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    if (!m_direct_present || m_direct_composite)
    {
        SDL_Surface* texture_surface;
        if (!SDL_LockTextureToSurface(m_texture, NULL, &texture_surface))
        {
            ERRORLOG("Present texture lock failed: %s", SDL_GetError());
            LbMouseOnEndSwap();
            return;
        }
        if (!SDL_BlitSurface(draw_surface, NULL, texture_surface, NULL))
        {
            ERRORLOG("Present blit failed: %s", SDL_GetError());
            SDL_UnlockTexture(m_texture);
            LbMouseOnEndSwap();
            return;
        }
        SDL_UnlockTexture(m_texture);

        const SDL_FRect dst = { (float)px, (float)py, (float)pw, (float)ph };
        SDL_SetTextureScaleMode(m_texture, start_params.enhanced_mode ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        SDL_RenderTexture(m_renderer, m_texture, NULL, (pw > 0 && ph > 0) ? &dst : NULL);
    }

    if (m_direct_present)
    {
        if (m_direct_texture == nullptr || m_direct_alloc_w != m_direct_tex_w || m_direct_alloc_h != m_direct_tex_h)
        {
            if (m_direct_texture != nullptr)
                SDL_DestroyTexture(m_direct_texture);
            m_direct_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 m_direct_tex_w, m_direct_tex_h);
            if (m_direct_texture != nullptr)
            {
                m_direct_alloc_w = m_direct_tex_w;
                m_direct_alloc_h = m_direct_tex_h;
            }
        }

        if (m_direct_texture != nullptr)
        {
            SDL_SetTextureScaleMode(m_direct_texture,
                (m_direct_filter == PRESENT_FILTER_NEAREST) ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
        }

        if (m_direct_texture != nullptr &&
            SDL_UpdateTexture(m_direct_texture, NULL, m_direct_rgba.data(), m_direct_tex_w * 4))
        {
            const float sx = (draw_surface->w > 0) ? (float)pw / (float)draw_surface->w : 1.0f;
            const float sy = (draw_surface->h > 0) ? (float)ph / (float)draw_surface->h : 1.0f;
            const SDL_FRect dst = {
                (float)px + (float)m_direct_dst_x * sx,
                (float)py + (float)m_direct_dst_y * sy,
                (float)m_direct_dst_w * sx,
                (float)m_direct_dst_h * sy
            };
            SDL_RenderTexture(m_renderer, m_direct_texture, NULL, &dst);
        }
        else if (m_direct_texture == nullptr)
        {
            ERRORLOG("Enhanced FMV texture creation failed: %s", SDL_GetError());
        }
        else
        {
            ERRORLOG("Enhanced FMV texture update failed: %s", SDL_GetError());
        }

        m_direct_present = false;
    }

    SDL_RenderPresent(m_renderer);
    LbMouseOnEndSwap();
}

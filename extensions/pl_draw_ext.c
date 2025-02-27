/*
   pl_draw_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] context
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] internal api implementation
// [SECTION] default font stuff
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include <math.h>
#include <stdio.h>
#include <float.h> // FLT_MAX
#include "stb_rect_pack.h"
#include "stb_truetype.h"
#include "pl_draw_ext.h"
#include "pl_ds.h"
#include "pl_string.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_METAL_BACKEND
#include "../backends/pl_metal.m"
#endif

#ifdef PL_VULKAN_BACKEND
#include "../backends/pl_vulkan.c"
#endif

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

static plDrawContext* gptDrawCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plFontPrepData
{
    stbtt_fontinfo    fontInfo;
    stbtt_pack_range* ranges;
    stbrp_rect*       rects;
    unsigned char*    ptrTtf;
    uint32_t          uTotalCharCount;
    float             scale;
    uint32_t          area;
} plFontPrepData;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// context
static void            pl__set_draw_context(plDrawContext* ptCtx);
static plDrawContext*  pl__get_draw_context(void);
// static void            pl__cleanup_draw_context(plDrawContext* ptCtx);  // implemented by backend

// setup
static void            pl__register_drawlist   (plDrawContext* ptCtx, plDrawList* ptDrawlist);
static void            pl__register_3d_drawlist(plDrawContext* ptCtx, plDrawList3D* ptDrawlist);
static plDrawLayer*    pl__request_draw_layer  (plDrawList* ptDrawlist, const char* pcName);
static void            pl__return_draw_layer   (plDrawLayer* ptLayer);

// per frame
// static void            pl__new_draw_frame   (plDrawContext* ptCtx); // implemented by backend
static void            pl__submit_draw_layer(plDrawLayer* ptLayer);

// drawing
static void            pl__add_line               (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec4 tColor, float fThickness);
static void            pl__add_lines              (plDrawLayer* ptLayer, plVec2* atPoints, uint32_t uCount, plVec4 tColor, float fThickness);
static void            pl__add_text               (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
static void            pl__add_text_ex            (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
static void            pl__add_text_clipped       (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
static void            pl__add_text_clipped_ex    (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
static void            pl__add_triangle           (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness);
static void            pl__add_triangle_filled    (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor);
static void            pl__add_rect               (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness);
static void            pl__add_rect_filled        (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor);
static void            pl__add_rect_rounded       (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments);
static void            pl__add_rect_rounded_filled(plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments);
static void            pl__add_quad               (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness);
static void            pl__add_quad_filled        (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor);
static void            pl__add_circle             (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
static void            pl__add_circle_filled      (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments);
static void            pl__add_image              (plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax);
static void            pl__add_image_ex           (plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, plVec4 tColor);
static void            pl__add_bezier_quad        (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
static void            pl__add_bezier_cubic       (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

// 3D drawing
static void            pl__add_3d_triangle_filled(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
static void            pl__add_3d_line           (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
static void            pl__add_3d_point          (plDrawList3D* ptDrawlist, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
static void            pl__add_3d_transform      (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness);
static void            pl__add_3d_frustum        (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
static void            pl__add_3d_centered_box   (plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
static void            pl__add_3d_bezier_quad    (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
static void            pl__add_3d_bezier_cubic   (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

// fonts
// static void            pl__build_font_atlas        (plDrawContext* ptCtx, plFontAtlas* ptAtlas); // implemented by backend
// static void            pl__cleanup_font_atlas      (plFontAtlas* ptAtlas);                     // implemented by backend
static void            pl__add_default_font        (plFontAtlas* ptAtlas);
static void            pl__add_font_from_file_ttf  (plFontAtlas* ptAtlas, plFontConfig tConfig, const char* pcFile);
static void            pl__add_font_from_memory_ttf(plFontAtlas* ptAtlas, plFontConfig tConfig, void* pData);
static plVec2          pl__calculate_text_size     (plFont* ptFont, float fSize, const char* pcText, float fWrap);
static plVec2          pl__calculate_text_size_ex  (plFont* ptFont, float fSize, const char* pcText, const char* pcTextEnd, float fWrap);
static plRect          pl__calculate_text_bb       (plFont* ptFont, float fSize, plVec2 tP, const char* pcText, float fWrap);
static plRect          pl__calculate_text_bb_ex    (plFont* ptFont, float fSize, plVec2 tP, const char* pcText, const char* pcTextEnd, float fWrap);

// clipping
static void            pl__push_clip_rect_pt       (plDrawList* ptDrawlist, const plRect* ptRect);
static void            pl__push_clip_rect          (plDrawList* ptDrawlist, plRect tRect, bool bAccumulate);
static void            pl__pop_clip_rect           (plDrawList* ptDrawlist);
static const plRect*   pl__get_clip_rect           (plDrawList* ptDrawlist);

// helpers
static void  pl__prepare_draw_command(plDrawLayer* layer, plTextureId texture, bool sdf);
static void  pl__reserve_triangles(plDrawLayer* layer, uint32_t indexCount, uint32_t vertexCount);
static void  pl__add_vertex(plDrawLayer* layer, plVec2 pos, plVec4 color, plVec2 uv);
static void  pl__add_index(plDrawLayer* layer, uint32_t vertexStart, uint32_t i0, uint32_t i1, uint32_t i2);
static float pl__get_max(float v1, float v2) { return v1 > v2 ? v1 : v2;}
static int   pl__get_min(int v1, int v2)     { return v1 < v2 ? v1 : v2;}
static char* pl__read_file(const char* file);

// math
#define pl__add_vec2(left, right)      (plVec2){(left).x + (right).x, (left).y + (right).y}
#define pl__subtract_vec2(left, right) (plVec2){(left).x - (right).x, (left).y - (right).y}
#define pl__mul_vec2_f(left, right)    (plVec2){(left).x * (right), (left).y * (right)}
#define pl__mul_f_vec2(left, right)    (plVec2){(left) * (right).x, (left) * (right).y}

// stateful drawing
#define pl__submit_path(layer, color, thickness)\
    pl__add_lines((layer), (layer)->sbPath, pl_sb_size((layer)->sbPath) - 1, (color), (thickness));\
    pl_sb_reset((layer)->sbPath);

#define PL_NORMALIZE2F_OVER_ZERO(VX,VY) \
    { float d2 = (VX) * (VX) + (VY) * (VY); \
    if (d2 > 0.0f) { float inv_len = 1.0f / sqrtf(d2); (VX) *= inv_len; (VY) *= inv_len; } } (void)0

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__set_draw_context(plDrawContext* ptCtx)
{
    gptDrawCtx = ptCtx;
}

static plDrawContext*
pl__get_draw_context(void)
{
    if(gptDrawCtx == NULL)
    {
        static plDrawContext tContext = {0};
        gptDrawCtx = &tContext;
    }
    return gptDrawCtx;
}

static void
pl__register_drawlist(plDrawContext* ctx, plDrawList* drawlist)
{
    memset(drawlist, 0, sizeof(plDrawList));
    drawlist->ctx = ctx;
    pl_sb_push(ctx->sbDrawlists, drawlist);
}

static void
pl__register_3d_drawlist(plDrawContext* ptCtx, plDrawList3D* ptDrawlist)
{
    memset(ptDrawlist, 0, sizeof(plDrawList3D));
    ptDrawlist->ctx = ptCtx;
    pl_sb_push(ptCtx->sb3DDrawlists, ptDrawlist);
}

static plDrawLayer*
pl__request_draw_layer(plDrawList* drawlist, const char* name)
{
   plDrawLayer* layer = NULL;
   
   // check if drawlist has any cached layers
   // which reduces allocations necessary since
   // cached layers' buffers are only reset
   if(pl_sb_size(drawlist->sbLayerCache) > 0)
   {
        layer = pl_sb_pop(drawlist->sbLayerCache);
   }

   else // create new layer
   {
        drawlist->layersCreated++;
        layer = PL_ALLOC(sizeof(plDrawLayer));
        memset(layer, 0, sizeof(plDrawLayer));
        layer->drawlist = drawlist;
        pl_sb_push(drawlist->sbLayersCreated, layer);
   }

   if(name)
      strncpy(layer->name, name, PL_MAX_NAME_LENGTH);

   return layer;
}

static void
pl__return_draw_layer(plDrawLayer* layer)
{
    layer->name[0] = 0;
    layer->_lastCommand = NULL;
    layer->vertexCount = 0u;
    pl_sb_reset(layer->sbCommandBuffer);
    pl_sb_reset(layer->sbIndexBuffer);
    pl_sb_reset(layer->sbPath);
    pl_sb_push(layer->drawlist->sbLayerCache, layer);
}

static void
pl__new_draw_frame_i(plDrawContext* ctx)
{
    gptDrawCtx = ctx;

    // reset drawlists
    for(uint32_t i = 0u; i < pl_sb_size(ctx->sbDrawlists); i++)
    {
        plDrawList* drawlist = ctx->sbDrawlists[i];

        drawlist->indexBufferByteSize = 0u;
        pl_sb_reset(drawlist->sbDrawCommands);
        pl_sb_reset(drawlist->sbVertexBuffer);

        // reset submitted layers
        for(uint32_t j = 0; j < pl_sb_size(drawlist->sbSubmittedLayers); j++)
        {
            pl_sb_reset(drawlist->sbSubmittedLayers[j]->sbCommandBuffer);
            pl_sb_reset(drawlist->sbSubmittedLayers[j]->sbIndexBuffer);   
            pl_sb_reset(drawlist->sbSubmittedLayers[j]->sbPath);  
            drawlist->sbSubmittedLayers[j]->vertexCount = 0u;
            drawlist->sbSubmittedLayers[j]->_lastCommand = NULL;
        }
        pl_sb_reset(drawlist->sbSubmittedLayers);       
    }

    // reset 3d drawlists
    for(uint32_t i = 0u; i < pl_sb_size(ctx->sb3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ctx->sb3DDrawlists[i];

        pl_sb_reset(drawlist->sbVertexBuffer);
        pl_sb_reset(drawlist->sbLineVertexBuffer);
        pl_sb_reset(drawlist->sbIndexBuffer);    
        pl_sb_reset(drawlist->sbLineIndexBuffer);    
    }

    ctx->frameCount++;
}

static void
pl__submit_draw_layer(plDrawLayer* layer)
{
    pl_sb_push(layer->drawlist->sbSubmittedLayers, layer);
    layer->drawlist->indexBufferByteSize += pl_sb_size(layer->sbIndexBuffer) * sizeof(uint32_t);
}

static void
pl__add_line(plDrawLayer* layer, plVec2 p0, plVec2 p1, plVec4 color, float thickness)
{
    pl_sb_push(layer->sbPath, p0);
    pl_sb_push(layer->sbPath, p1);
    pl__submit_path(layer, color, thickness);
}

static void
pl__add_lines(plDrawLayer* layer, plVec2* points, uint32_t count, plVec4 color, float thickness)
{
    pl__prepare_draw_command(layer, layer->drawlist->ctx->fontAtlas->texture, false);
    pl__reserve_triangles(layer, 6 * count, 4 * count);

    for(uint32_t i = 0u; i < count; i++)
    {
        float dx = points[i + 1].x - points[i].x;
        float dy = points[i + 1].y - points[i].y;
        PL_NORMALIZE2F_OVER_ZERO(dx, dy);

        plVec2 normalVector = 
        {
            .x = dy,
            .y = -dx
        };

        plVec2 cornerPoints[4] = 
        {
            pl__subtract_vec2(points[i],     pl__mul_vec2_f(normalVector, thickness / 2.0f)),
            pl__subtract_vec2(points[i + 1], pl__mul_vec2_f(normalVector, thickness / 2.0f)),
            pl__add_vec2(     points[i + 1], pl__mul_vec2_f(normalVector, thickness / 2.0f)),
            pl__add_vec2(     points[i],     pl__mul_vec2_f(normalVector, thickness / 2.0f))
        };

        uint32_t vertexStart = pl_sb_size(layer->drawlist->sbVertexBuffer);
        pl__add_vertex(layer, cornerPoints[0], color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
        pl__add_vertex(layer, cornerPoints[1], color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
        pl__add_vertex(layer, cornerPoints[2], color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
        pl__add_vertex(layer, cornerPoints[3], color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});

        pl__add_index(layer, vertexStart, 0, 1, 2);
        pl__add_index(layer, vertexStart, 0, 2, 3);
    }  
}

static void
pl__add_text(plDrawLayer* layer, plFont* font, float size, plVec2 p, plVec4 color, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    pl__add_text_ex(layer, font, size, p, color, text, pcTextEnd, wrap);
}

static void
pl__add_text_ex(plDrawLayer* layer, plFont* font, float size, plVec2 p, plVec4 color, const char* text, const char* pcTextEnd, float wrap)
{
    float scale = size > 0.0f ? size / font->config.fontSize : 1.0f;

    float lineSpacing = scale * font->lineSpacing;
    const plVec2 originalPosition = p;
    bool firstCharacter = true;

    while(text < pcTextEnd)
    {
        uint32_t c = (uint32_t)*text;
        if(c < 0x80)
            text += 1;
        else
        {
            text += pl_text_char_from_utf8(&c, text, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            p.x = originalPosition.x;
            p.y += lineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else
        {

            bool glyphFound = false;
            for(uint32_t i = 0u; i < pl_sb_size(font->config.sbRanges); i++)
            {
                if (c >= (uint32_t)font->config.sbRanges[i].firstCodePoint && c < (uint32_t)font->config.sbRanges[i].firstCodePoint + (uint32_t)font->config.sbRanges[i].charCount) 
                {

                    
                    float x0,y0,s0,t0; // top-left
                    float x1,y1,s1,t1; // bottom-right

                    const plFontGlyph* glyph = &font->sbGlyphs[font->sbCodePoints[c]];

                    // adjust for left side bearing if first char
                    if(firstCharacter)
                    {
                        if(glyph->leftBearing > 0.0f) p.x += glyph->leftBearing * scale;
                        firstCharacter = false;
                    }

                    x0 = p.x + glyph->x0 * scale;
                    x1 = p.x + glyph->x1 * scale;
                    y0 = p.y + glyph->y0 * scale;
                    y1 = p.y + glyph->y1 * scale;

                    if(wrap > 0.0f && x1 > originalPosition.x + wrap)
                    {
                        x0 = originalPosition.x + glyph->x0 * scale;
                        y0 = y0 + lineSpacing;
                        x1 = originalPosition.x + glyph->x1 * scale;
                        y1 = y1 + lineSpacing;

                        p.x = originalPosition.x;
                        p.y += lineSpacing;
                    }
                    s0 = glyph->u0;
                    t0 = glyph->v0;
                    s1 = glyph->u1;
                    t1 = glyph->v1;

                    p.x += glyph->xAdvance * scale;
                    if(c != ' ')
                    {
                        pl__prepare_draw_command(layer, font->parentAtlas->texture, font->config.sdf);
                        pl__reserve_triangles(layer, 6, 4);
                        uint32_t uVtxStart = pl_sb_size(layer->drawlist->sbVertexBuffer);
                        pl__add_vertex(layer, (plVec2){x0, y0}, color, (plVec2){s0, t0});
                        pl__add_vertex(layer, (plVec2){x1, y0}, color, (plVec2){s1, t0});
                        pl__add_vertex(layer, (plVec2){x1, y1}, color, (plVec2){s1, t1});
                        pl__add_vertex(layer, (plVec2){x0, y1}, color, (plVec2){s0, t1});

                        pl__add_index(layer, uVtxStart, 1, 0, 2);
                        pl__add_index(layer, uVtxStart, 2, 0, 3);
                    }

                    glyphFound = true;
                    break;
                }
            }

            PL_ASSERT(glyphFound && "Glyph not found");
        }   
    }
}

static void
pl__add_text_clipped(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    pl__add_text_clipped_ex(ptLayer, ptFont, fSize, tP, tMin, tMax, tColor, pcText, pcTextEnd, fWrap);
}

static void
pl__add_text_clipped_ex(plDrawLayer* layer, plFont* font, float size, plVec2 p, plVec2 tMin, plVec2 tMax, plVec4 color, const char* text, const char* pcTextEnd, float wrap)
{
    // const plVec2 tTextSize = pl_calculate_text_size_ex(font, size, text, pcTextEnd, wrap);
    const plRect tClipRect = {tMin, tMax};

    float scale = size > 0.0f ? size / font->config.fontSize : 1.0f;

    float lineSpacing = scale * font->lineSpacing;
    const plVec2 originalPosition = p;
    bool firstCharacter = true;

    while(text < pcTextEnd)
    {
        uint32_t c = (uint32_t)*text;
        if(c < 0x80)
            text += 1;
        else
        {
            text += pl_text_char_from_utf8(&c, text, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            p.x = originalPosition.x;
            p.y += lineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else
        {

            bool glyphFound = false;
            for(uint32_t i = 0u; i < pl_sb_size(font->config.sbRanges); i++)
            {
                if (c >= (uint32_t)font->config.sbRanges[i].firstCodePoint && c < (uint32_t)font->config.sbRanges[i].firstCodePoint + (uint32_t)font->config.sbRanges[i].charCount) 
                {

                    
                    float x0,y0,s0,t0; // top-left
                    float x1,y1,s1,t1; // bottom-right

                    const plFontGlyph* glyph = &font->sbGlyphs[font->sbCodePoints[c]];

                    // adjust for left side bearing if first char
                    if(firstCharacter)
                    {
                        if(glyph->leftBearing > 0.0f) p.x += glyph->leftBearing * scale;
                        firstCharacter = false;
                    }

                    x0 = p.x + glyph->x0 * scale;
                    x1 = p.x + glyph->x1 * scale;
                    y0 = p.y + glyph->y0 * scale;
                    y1 = p.y + glyph->y1 * scale;

                    if(wrap > 0.0f && x1 > originalPosition.x + wrap)
                    {
                        x0 = originalPosition.x + glyph->x0 * scale;
                        y0 = y0 + lineSpacing;
                        x1 = originalPosition.x + glyph->x1 * scale;
                        y1 = y1 + lineSpacing;

                        p.x = originalPosition.x;
                        p.y += lineSpacing;
                    }
                    s0 = glyph->u0;
                    t0 = glyph->v0;
                    s1 = glyph->u1;
                    t1 = glyph->v1;

                    p.x += glyph->xAdvance * scale;
                    if(c != ' ' && pl_rect_contains_point(&tClipRect, p))
                    {
                        pl__prepare_draw_command(layer, font->parentAtlas->texture, font->config.sdf);
                        pl__reserve_triangles(layer, 6, 4);
                        uint32_t uVtxStart = pl_sb_size(layer->drawlist->sbVertexBuffer);
                        pl__add_vertex(layer, (plVec2){x0, y0}, color, (plVec2){s0, t0});
                        pl__add_vertex(layer, (plVec2){x1, y0}, color, (plVec2){s1, t0});
                        pl__add_vertex(layer, (plVec2){x1, y1}, color, (plVec2){s1, t1});
                        pl__add_vertex(layer, (plVec2){x0, y1}, color, (plVec2){s0, t1});

                        pl__add_index(layer, uVtxStart, 1, 0, 2);
                        pl__add_index(layer, uVtxStart, 2, 0, 3);
                    }

                    glyphFound = true;
                    break;
                }
            }

            PL_ASSERT(glyphFound && "Glyph not found");
        }   
    }   
}

static void
pl__add_triangle(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness)
{
    pl_sb_push(ptLayer->sbPath, tP0);
    pl_sb_push(ptLayer->sbPath, tP1);
    pl_sb_push(ptLayer->sbPath, tP2);
    pl_sb_push(ptLayer->sbPath, tP0);
    pl__submit_path(ptLayer, tColor, fThickness);    
}

static void
pl__add_triangle_filled(plDrawLayer* layer, plVec2 p0, plVec2 p1, plVec2 p2, plVec4 color)
{
    pl__prepare_draw_command(layer, layer->drawlist->ctx->fontAtlas->texture, false);
    pl__reserve_triangles(layer, 3, 3);

    uint32_t vertexStart = pl_sb_size(layer->drawlist->sbVertexBuffer);
    pl__add_vertex(layer, p0, color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
    pl__add_vertex(layer, p1, color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
    pl__add_vertex(layer, p2, color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});

    pl__add_index(layer, vertexStart, 0, 1, 2);
}

static void
pl__add_rect(plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness)
{
    const plVec2 fBotLeftVec  = {tMinP.x, tMaxP.y};
    const plVec2 fTopRightVec = {tMaxP.x, tMinP.y};

    pl_sb_push(ptLayer->sbPath, tMinP);
    pl_sb_push(ptLayer->sbPath, fBotLeftVec);
    pl_sb_push(ptLayer->sbPath, tMaxP);
    pl_sb_push(ptLayer->sbPath, fTopRightVec);
    pl_sb_push(ptLayer->sbPath, tMinP);
    pl__submit_path(ptLayer, tColor, fThickness);   
}

static void
pl__add_rect_filled(plDrawLayer* layer, plVec2 minP, plVec2 maxP, plVec4 color)
{
    pl__prepare_draw_command(layer, layer->drawlist->ctx->fontAtlas->texture, false);
    pl__reserve_triangles(layer, 6, 4);

    const plVec2 bottomLeft = { minP.x, maxP.y };
    const plVec2 topRight =   { maxP.x, minP.y };

    const uint32_t vertexStart = pl_sb_size(layer->drawlist->sbVertexBuffer);
    pl__add_vertex(layer, minP,       color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
    pl__add_vertex(layer, bottomLeft, color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
    pl__add_vertex(layer, maxP,       color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});
    pl__add_vertex(layer, topRight,   color, (plVec2){layer->drawlist->ctx->fontAtlas->whiteUv[0], layer->drawlist->ctx->fontAtlas->whiteUv[1]});

    pl__add_index(layer, vertexStart, 0, 1, 2);
    pl__add_index(layer, vertexStart, 0, 2, 3);
}

// segments is the number of segments used to approximate one corner
static void
pl__add_rect_rounded(plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments)
{
    if(uSegments == 0){ uSegments = 3; }
    const float fIncrement = PL_PI_2 / uSegments;
    float fTheta = 0.0f;


    const plVec2 bottomRightStart = { tMaxP.x, tMaxP.y - fRadius };
    const plVec2 bottomRightInner = { tMaxP.x - fRadius, tMaxP.y - fRadius };
    const plVec2 bottomRightEnd   = { tMaxP.x - fRadius, tMaxP.y };

    const plVec2 bottomLeftStart  = { tMinP.x + fRadius, tMaxP.y };
    const plVec2 bottomLeftInner  = { tMinP.x + fRadius, tMaxP.y - fRadius };
    const plVec2 bottomLeftEnd    = { tMinP.x , tMaxP.y - fRadius};
 
    const plVec2 topLeftStart     = { tMinP.x, tMinP.y + fRadius };
    const plVec2 topLeftInner     = { tMinP.x + fRadius, tMinP.y + fRadius };
    const plVec2 topLeftEnd       = { tMinP.x + fRadius, tMinP.y };

    const plVec2 topRightStart    = { tMaxP.x - fRadius, tMinP.y };
    const plVec2 topRightInner    = { tMaxP.x - fRadius, tMinP.y + fRadius };
    const plVec2 topRightEnd      = { tMaxP.x, tMinP.y + fRadius };
    
    pl_sb_push(ptLayer->sbPath, bottomRightStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbPath, ((plVec2){bottomRightInner.x + fRadius * sinf(fTheta + PL_PI_2), bottomRightInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbPath, bottomRightEnd);

    pl_sb_push(ptLayer->sbPath, bottomLeftStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbPath, ((plVec2){bottomLeftInner.x + fRadius * sinf(fTheta + PL_PI_2), bottomLeftInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbPath, bottomLeftEnd);

    pl_sb_push(ptLayer->sbPath, topLeftStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbPath, ((plVec2){topLeftInner.x + fRadius * sinf(fTheta + PL_PI_2), topLeftInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbPath, topLeftEnd);

    pl_sb_push(ptLayer->sbPath, topRightStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbPath, ((plVec2){topRightInner.x + fRadius * sinf(fTheta + PL_PI_2), topRightInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbPath, topRightEnd);

    pl_sb_push(ptLayer->sbPath, bottomRightStart);

    pl__submit_path(ptLayer, tColor, fThickness);
}

// segments is the number of segments used to approximate one corner
static void
pl__add_rect_rounded_filled(plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments)
{
    if(uSegments == 0){ uSegments = 3; }
    const uint32_t numTriangles = (uSegments * 4 + 4); //number segments in midpoint circle, plus square
    pl__prepare_draw_command(ptLayer, ptLayer->drawlist->ctx->fontAtlas->texture, false);
    pl__reserve_triangles(ptLayer, numTriangles, numTriangles + 1);

    const uint32_t uVertexStart = pl_sb_size(ptLayer->drawlist->sbVertexBuffer);

    const float fIncrement = PL_PI_2 / uSegments;
    float fTheta = 0.0f;

    const plVec2 bottomRightStart = { tMaxP.x, tMaxP.y - fRadius };
    const plVec2 bottomRightInner = { tMaxP.x - fRadius, tMaxP.y - fRadius };
    const plVec2 bottomRightEnd   = { tMaxP.x - fRadius, tMaxP.y };

    const plVec2 bottomLeftStart  = { tMinP.x + fRadius, tMaxP.y };
    const plVec2 bottomLeftInner  = { tMinP.x + fRadius, tMaxP.y - fRadius };
    const plVec2 bottomLeftEnd    = { tMinP.x , tMaxP.y - fRadius};
 
    const plVec2 topLeftStart     = { tMinP.x, tMinP.y + fRadius };
    const plVec2 topLeftInner     = { tMinP.x + fRadius, tMinP.y + fRadius };
    const plVec2 topLeftEnd       = { tMinP.x + fRadius, tMinP.y };

    const plVec2 topRightStart    = { tMaxP.x - fRadius, tMinP.y };
    const plVec2 topRightInner    = { tMaxP.x - fRadius, tMinP.y + fRadius };
    const plVec2 topRightEnd      = { tMaxP.x, tMinP.y + fRadius };

    const plVec2 midPoint = {(tMaxP.x-tMinP.x)/2 + tMinP.x, (tMaxP.y-tMinP.y)/2 + tMinP.y};
    pl__add_vertex(ptLayer, midPoint, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});

    pl__add_vertex(ptLayer, bottomRightStart, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl__add_vertex(ptLayer, ((plVec2){bottomRightInner.x + fRadius * sinf(fTheta + PL_PI_2), bottomRightInner.y + fRadius * sinf(fTheta)}), tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
        fTheta += fIncrement;
    }
    pl__add_vertex(ptLayer, bottomRightEnd, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});

    pl__add_vertex(ptLayer, bottomLeftStart, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl__add_vertex(ptLayer, ((plVec2){bottomLeftInner.x + fRadius * sinf(fTheta + PL_PI_2), bottomLeftInner.y + fRadius * sinf(fTheta)}), tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
        fTheta += fIncrement;
    }
    pl__add_vertex(ptLayer, bottomLeftEnd, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});

    pl__add_vertex(ptLayer, topLeftStart, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl__add_vertex(ptLayer, ((plVec2){topLeftInner.x + fRadius * sinf(fTheta + PL_PI_2), topLeftInner.y + fRadius * sinf(fTheta)}), tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
        fTheta += fIncrement;
    }
    pl__add_vertex(ptLayer, topLeftEnd, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});

    pl__add_vertex(ptLayer, topRightStart, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl__add_vertex(ptLayer, ((plVec2){topRightInner.x + fRadius * sinf(fTheta + PL_PI_2), topRightInner.y + fRadius * sinf(fTheta)}), tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
        fTheta += fIncrement;
    }
    pl__add_vertex(ptLayer, topRightEnd, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});

    for(uint32_t i = 0; i < numTriangles - 1; i++)
        pl__add_index(ptLayer, uVertexStart, i + 1, 0, i + 2);
    pl__add_index(ptLayer, uVertexStart, numTriangles, 0, 1);

}

static void
pl__add_quad(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness)
{
    pl_sb_push(ptLayer->sbPath, tP0);
    pl_sb_push(ptLayer->sbPath, tP1);
    pl_sb_push(ptLayer->sbPath, tP2);
    pl_sb_push(ptLayer->sbPath, tP3);
    pl_sb_push(ptLayer->sbPath, tP0);
    pl__submit_path(ptLayer, tColor, fThickness);
}

static void
pl__add_quad_filled(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor)
{
    pl__prepare_draw_command(ptLayer, ptLayer->drawlist->ctx->fontAtlas->texture, false);
    pl__reserve_triangles(ptLayer, 6, 4);

    const uint32_t uVtxStart = pl_sb_size(ptLayer->drawlist->sbVertexBuffer);
    pl__add_vertex(ptLayer, tP0, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]}); // top left
    pl__add_vertex(ptLayer, tP1, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]}); // bot left
    pl__add_vertex(ptLayer, tP2, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]}); // bot right
    pl__add_vertex(ptLayer, tP3, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]}); // top right

    pl__add_index(ptLayer, uVtxStart, 0, 1, 2);
    pl__add_index(ptLayer, uVtxStart, 0, 2, 3);
}

static void
pl__add_circle(plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness)
{
    if(uSegments == 0){ uSegments = 12; }
    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbPath, ((plVec2){tP.x + fRadius * sinf(fTheta + PL_PI_2), tP.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbPath, ((plVec2){tP.x + fRadius, tP.y}));
    pl__submit_path(ptLayer, tColor, fThickness);   
}

static void
pl__add_circle_filled(plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments)
{
    if(uSegments == 0){ uSegments = 12; }
    pl__prepare_draw_command(ptLayer, ptLayer->drawlist->ctx->fontAtlas->texture, false);
    pl__reserve_triangles(ptLayer, 3 * uSegments, uSegments + 1);

    const uint32_t uVertexStart = pl_sb_size(ptLayer->drawlist->sbVertexBuffer);
    pl__add_vertex(ptLayer, tP, tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        pl__add_vertex(ptLayer, ((plVec2){tP.x + (fRadius * sinf(fTheta + PL_PI_2)), tP.y + (fRadius * sinf(fTheta))}), tColor, (plVec2){ptLayer->drawlist->ctx->fontAtlas->whiteUv[0], ptLayer->drawlist->ctx->fontAtlas->whiteUv[1]});
        fTheta += fIncrement;
    }

    for(uint32_t i = 0; i < uSegments - 1; i++)
        pl__add_index(ptLayer, uVertexStart, i + 1, 0, i + 2);
    pl__add_index(ptLayer, uVertexStart, uSegments, 0, 1);
}

static void
pl__add_image(plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax)
{
    pl__add_image_ex(ptLayer, tTexture, tPMin, tPMax, (plVec2){0}, (plVec2){1.0f, 1.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
}

static void
pl__add_image_ex(plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, plVec4 tColor)
{
    pl__prepare_draw_command(ptLayer, tTexture, false);
    pl__reserve_triangles(ptLayer, 6, 4);

    const plVec2 bottomLeft = { tPMin.x, tPMax.y };
    const plVec2 topRight =   { tPMax.x, tPMin.y };

    const uint32_t vertexStart = pl_sb_size(ptLayer->drawlist->sbVertexBuffer);
    pl__add_vertex(ptLayer, tPMin,       tColor, tUvMin);
    pl__add_vertex(ptLayer, bottomLeft, tColor, (plVec2){tUvMin.x, tUvMax.y});
    pl__add_vertex(ptLayer, tPMax,       tColor, tUvMax);
    pl__add_vertex(ptLayer, topRight,   tColor, (plVec2){tUvMax.x, tUvMin.y});

    pl__add_index(ptLayer, vertexStart, 0, 1, 2);
    pl__add_index(ptLayer, vertexStart, 0, 2, 3);
}

// order of the bezier curve inputs are 0=start, 1=control, 2=ending
static void
pl__add_bezier_quad(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness, uint32_t uSegments)
{

    if(uSegments == 0)
        uSegments = 12;

    // push first point
    pl_sb_push(ptLayer->sbPath, tP0);

    // calculate and push points between first and last
    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        
        const plVec2 p0 = pl_mul_vec2_scalarf(tP0, uu);
        const plVec2 p1 = pl_mul_vec2_scalarf(tP1, (2.0f * u * t)); 
        const plVec2 p2 = pl_mul_vec2_scalarf(tP2, tt); 
        const plVec2 p3 = pl_add_vec2(p0,p1);
        const plVec2 p4 = pl_add_vec2(p2,p3);

        pl_sb_push(ptLayer->sbPath, p4);
    }

    // push last point
    pl_sb_push(ptLayer->sbPath, tP2);

    pl__submit_path(ptLayer, tColor, fThickness); 
}

// order of the bezier curve inputs are 0=start, 1=control 1, 2=control 2, 3=ending
static void
pl__add_bezier_cubic(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness, uint32_t uSegments)
{

    if(uSegments == 0)
        uSegments = 12;

    // push first point
    pl_sb_push(ptLayer->sbPath, tP0);

    // calculate and push points between first and last
    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        const float uuu = uu * u;
        const float ttt = tt * t;
        
        const plVec2 p0 = pl_mul_vec2_scalarf(tP0, uuu);
        const plVec2 p1 = pl_mul_vec2_scalarf(tP1, (3.0f * uu * t)); 
        const plVec2 p2 = pl_mul_vec2_scalarf(tP2, (3.0f * u * tt)); 
        const plVec2 p3 = pl_mul_vec2_scalarf(tP3, (ttt));
        const plVec2 p5 = pl_add_vec2(p0,p1);
        const plVec2 p6 = pl_add_vec2(p2,p3);
        const plVec2 p7 = pl_add_vec2(p5,p6);

        pl_sb_push(ptLayer->sbPath, p7);
    }

    // push last point
    pl_sb_push(ptLayer->sbPath, tP3);

    pl__submit_path(ptLayer, tColor, fThickness); 
}

static void
pl__add_3d_triangle_filled(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor)
{

    pl_sb_reserve(ptDrawlist->sbVertexBuffer, pl_sb_size(ptDrawlist->sbVertexBuffer) + 3);
    pl_sb_reserve(ptDrawlist->sbIndexBuffer, pl_sb_size(ptDrawlist->sbIndexBuffer) + 3);

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbVertexBuffer);

    uint32_t tU32Color = 0;
    tU32Color = (uint32_t)  (255.0f * tColor.r + 0.5f);
    tU32Color |= (uint32_t) (255.0f * tColor.g + 0.5f) << 8;
    tU32Color |= (uint32_t) (255.0f * tColor.b + 0.5f) << 16;
    tU32Color |= (uint32_t) (255.0f * tColor.a + 0.5f) << 24;

    pl_sb_push(ptDrawlist->sbVertexBuffer, ((plDrawVertex3D){ {tP0.x, tP0.y, tP0.z}, tU32Color}));
    pl_sb_push(ptDrawlist->sbVertexBuffer, ((plDrawVertex3D){ {tP1.x, tP1.y, tP1.z}, tU32Color}));
    pl_sb_push(ptDrawlist->sbVertexBuffer, ((plDrawVertex3D){ {tP2.x, tP2.y, tP2.z}, tU32Color}));

    pl_sb_push(ptDrawlist->sbIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbIndexBuffer, uVertexStart + 2);
}

static void
pl__add_3d_line(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness)
{
    uint32_t tU32Color = 0;
    tU32Color = (uint32_t)  (255.0f * tColor.r + 0.5f);
    tU32Color |= (uint32_t) (255.0f * tColor.g + 0.5f) << 8;
    tU32Color |= (uint32_t) (255.0f * tColor.b + 0.5f) << 16;
    tU32Color |= (uint32_t) (255.0f * tColor.a + 0.5f) << 24;

    pl_sb_reserve(ptDrawlist->sbLineVertexBuffer, pl_sb_size(ptDrawlist->sbLineVertexBuffer) + 4);
    pl_sb_reserve(ptDrawlist->sbLineIndexBuffer, pl_sb_size(ptDrawlist->sbLineIndexBuffer) + 6);

    plDrawVertex3DLine tNewVertex0 = {
        {tP0.x, tP0.y, tP0.z},
        -1.0f,
        fThickness,
        1.0f,
        {tP1.x, tP1.y, tP1.z},
        tU32Color
    };

    plDrawVertex3DLine tNewVertex1 = {
        {tP1.x, tP1.y, tP1.z},
        -1.0f,
        fThickness,
        -1.0f,
        {tP0.x, tP0.y, tP0.z},
        tU32Color
    };

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbLineVertexBuffer);
    pl_sb_push(ptDrawlist->sbLineVertexBuffer, tNewVertex0);
    pl_sb_push(ptDrawlist->sbLineVertexBuffer, tNewVertex1);

    tNewVertex0.fDirection = 1.0f;
    tNewVertex1.fDirection = 1.0f;
    pl_sb_push(ptDrawlist->sbLineVertexBuffer, tNewVertex1);
    pl_sb_push(ptDrawlist->sbLineVertexBuffer, tNewVertex0);

    pl_sb_push(ptDrawlist->sbLineIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbLineIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbLineIndexBuffer, uVertexStart + 2);

    pl_sb_push(ptDrawlist->sbLineIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbLineIndexBuffer, uVertexStart + 2);
    pl_sb_push(ptDrawlist->sbLineIndexBuffer, uVertexStart + 3);
}

static void
pl__add_3d_point(plDrawList3D* ptDrawlist, plVec3 tP, plVec4 tColor, float fLength, float fThickness)
{
    const plVec3 tVerticies[6] = {
        {  tP.x - fLength / 2.0f,  tP.y, tP.z},
        {  tP.x + fLength / 2.0f,  tP.y, tP.z},
        {  tP.x,  tP.y - fLength / 2.0f, tP.z},
        {  tP.x,  tP.y + fLength / 2.0f, tP.z},
        {  tP.x,  tP.y, tP.z - fLength / 2.0f},
        {  tP.x,  tP.y, tP.z + fLength / 2.0f}
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
}

static void
pl__add_3d_transform(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness)
{

    const plVec3 tOrigin = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, 0.0f});
    const plVec3 tXAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){fLength, 0.0f, 0.0f});
    const plVec3 tYAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, fLength, 0.0f});
    const plVec3 tZAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, fLength});

    pl__add_3d_line(ptDrawlist, tOrigin, tXAxis, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, fThickness);
    pl__add_3d_line(ptDrawlist, tOrigin, tYAxis, (plVec4){0.0f, 1.0f, 0.0f, 1.0f}, fThickness);
    pl__add_3d_line(ptDrawlist, tOrigin, tZAxis, (plVec4){0.0f, 0.0f, 1.0f, 1.0f}, fThickness);
}

static void
pl__add_3d_frustum(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness)
{
    const float fSmallHeight = tanf(fYFov / 2.0f) * fNearZ;
    const float fSmallWidth  = fSmallHeight * fAspect;
    const float fBigHeight   = tanf(fYFov / 2.0f) * fFarZ;
    const float fBigWidth    = fBigHeight * fAspect;

    const plVec3 tVerticies[8] = {
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth,  fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth, -fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth, -fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth,  fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,    fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,   -fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,   -fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,    fBigHeight,   fFarZ})
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[2], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[0], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[4], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[5], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[6], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[7], tVerticies[4], tColor, fThickness);
}

static void
pl__add_3d_centered_box(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness)
{
    const plVec3 tWidthVec  = {fWidth / 2.0f, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHeight / 2.0f, 0.0f};
    const plVec3 tDepthVec  = {0.0f, 0.0f, fDepth / 2.0f};

    const plVec3 tVerticies[8] = {
        {  tCenter.x - fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z + fDepth / 2.0f}
    };

    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[2], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[3], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[0], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[4], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[1], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[2], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[3], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[4], tVerticies[5], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[5], tVerticies[6], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[6], tVerticies[7], tColor, fThickness);
    pl__add_3d_line(ptDrawlist, tVerticies[7], tVerticies[4], tColor, fThickness);
}

// order of the bezier curve inputs are 0=start, 1=control, 2=ending
static void
pl__add_3d_bezier_quad(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments)
{

    if(uSegments == 0)
        uSegments = 12;

    // set up first point
    plVec3 tVerticies[2] = {(plVec3){0.0, 0.0, 0.0},tP0};

    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        
        const plVec3 p0 = pl_mul_vec3_scalarf(tP0, uu);
        const plVec3 p1 = pl_mul_vec3_scalarf(tP1, (2.0f * u * t)); 
        const plVec3 p2 = pl_mul_vec3_scalarf(tP2, tt); 
        const plVec3 p3 = pl_add_vec3(p0,p1);
        const plVec3 p4 = pl_add_vec3(p2,p3);
        
        // shift and add next point
        tVerticies[0] = tVerticies[1];
        tVerticies[1] = p4;

        pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    }

    // set up last point
    tVerticies[0] = tVerticies[1];
    tVerticies[1] = tP2;
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
}

// order of the bezier curve inputs are 0=start, 1=control 1, 2=control 2, 3=ending
static void
pl__add_3d_bezier_cubic(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments)
{

    if(uSegments == 0)
        uSegments = 12;

    // set up first point
    plVec3 tVerticies[2] = {(plVec3){0.0, 0.0, 0.0},tP0};

    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        const float uuu = uu * u;
        const float ttt = tt * t;
        
        const plVec3 p0 = pl_mul_vec3_scalarf(tP0, uuu);
        const plVec3 p1 = pl_mul_vec3_scalarf(tP1, (3.0f * uu * t)); 
        const plVec3 p2 = pl_mul_vec3_scalarf(tP2, (3.0f * u * tt)); 
        const plVec3 p3 = pl_mul_vec3_scalarf(tP3, (ttt));
        const plVec3 p5 = pl_add_vec3(p0,p1);
        const plVec3 p6 = pl_add_vec3(p2,p3);
        const plVec3 p7 = pl_add_vec3(p5,p6);
        
        // shift and add next point
        tVerticies[0] = tVerticies[1];
        tVerticies[1] = p7;

        pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
    }

    // set up last point
    tVerticies[0] = tVerticies[1];
    tVerticies[1] = tP3;
    pl__add_3d_line(ptDrawlist, tVerticies[0], tVerticies[1], tColor, fThickness);
}

static void
pl__add_font_from_file_ttf(plFontAtlas* atlas, plFontConfig config, const char* file)
{
    void* data = pl__read_file(file); // freed after atlas is created
    pl__add_font_from_memory_ttf(atlas, config, data);
}

static void
pl__add_font_from_memory_ttf(plFontAtlas* atlas, plFontConfig config, void* data)
{
    atlas->dirty = true;
    atlas->glyphPadding = 1;

    plFont font = 
    {
        .config = config
    };

    // prepare stb
    plFontPrepData prep = {0};
    stbtt_InitFont(&prep.fontInfo, (unsigned char*)data, 0);

    // get vertical font metrics
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&prep.fontInfo, &ascent, &descent, &lineGap);

    // calculate scaling factor
    prep.scale = 1.0f;
    if(font.config.fontSize > 0) prep.scale = stbtt_ScaleForPixelHeight(&prep.fontInfo, font.config.fontSize);
    else                         prep.scale = stbtt_ScaleForMappingEmToPixels(&prep.fontInfo, -font.config.fontSize);

    // calculate SDF pixel increment
    if(font.config.sdf) font.config.sdfPixelDistScale = (float)font.config.onEdgeValue / (float) font.config.sdfPadding;

    // calculate base line spacing
    int ascentBias = -1;
    int descentBias = -1;
    font.ascent = floorf(ascent * prep.scale - ascentBias);
    font.descent = floorf(descent * prep.scale - descentBias);
    font.lineSpacing = (font.ascent - font.descent + prep.scale * (float)lineGap);

    // convert individual chars to ranges
    for(uint32_t i = 0; i < pl_sb_size(font.config.sbIndividualChars); i++)
    {
        plFontRange range = {
            .charCount = 1,
            .firstCodePoint = font.config.sbIndividualChars[i],
            .ptrFontChar = NULL
        };
        pl_sb_push(font.config.sbRanges, range);
    }

    // find total number of glyphs/chars required
    uint32_t totalCharCount = 0u;
    for(uint32_t i = 0; i < pl_sb_size(font.config.sbRanges); i++)
        totalCharCount += font.config.sbRanges[i].charCount;
    
    pl_sb_reserve(font.sbGlyphs, totalCharCount);
    pl_sb_resize(font.sbCharData, totalCharCount);

    if(font.config.sdf)
        pl_sb_reserve(atlas->sbCustomRects, pl_sb_size(atlas->sbCustomRects) + totalCharCount); // is this correct

    prep.ranges = PL_ALLOC(sizeof(stbtt_pack_range) * pl_sb_size(font.config.sbRanges));
    memset(prep.ranges, 0, sizeof(stbtt_pack_range) * pl_sb_size(font.config.sbRanges));

    // find max codepoint & set range pointers into font char data
    int k = 0;
    int maxCodePoint = 0;
    totalCharCount = 0u;
    bool missingGlyphAdded = false;

    for(uint32_t i = 0; i < pl_sb_size(font.config.sbRanges); i++)
    {
        plFontRange* range = &font.config.sbRanges[i];
        prep.uTotalCharCount += range->charCount;
    }

    if(!font.config.sdf)
    {
        prep.rects = PL_ALLOC(sizeof(stbrp_rect) * prep.uTotalCharCount);
    }

    for(uint32_t i = 0; i < pl_sb_size(font.config.sbRanges); i++)
    {
        plFontRange* range = &font.config.sbRanges[i];

        if(range->firstCodePoint + (int)range->charCount > maxCodePoint)
            maxCodePoint = range->firstCodePoint + (int)range->charCount;

        range->ptrFontChar = &font.sbCharData[totalCharCount];

        // prepare stb stuff
        prep.ranges[i].font_size = font.config.fontSize;
        prep.ranges[i].first_unicode_codepoint_in_range = range->firstCodePoint;
        prep.ranges[i].chardata_for_range = (stbtt_packedchar*)range->ptrFontChar;
        prep.ranges[i].num_chars = range->charCount;
        prep.ranges[i].h_oversample = (unsigned char) font.config.hOverSampling;
        prep.ranges[i].v_oversample = (unsigned char) font.config.vOverSampling;

        // flag all characters as NOT packed
        memset(prep.ranges[i].chardata_for_range, 0, sizeof(stbtt_packedchar) * range->charCount);

        if(font.config.sdf)
        {
            for (uint32_t j = 0; j < (uint32_t)prep.ranges[i].num_chars; j++) 
            {
                int codePoint = 0;
                if(prep.ranges[i].array_of_unicode_codepoints) codePoint = prep.ranges[i].array_of_unicode_codepoints[j];
                else                                           codePoint = prep.ranges[i].first_unicode_codepoint_in_range + j;


                int width = 0u;
                int height = 0u;
                int xOff = 0u;
                int yOff = 0u;
                unsigned char* bytes = stbtt_GetCodepointSDF(&prep.fontInfo, stbtt_ScaleForPixelHeight(&prep.fontInfo, font.config.fontSize), codePoint, font.config.sdfPadding, font.config.onEdgeValue, font.config.sdfPixelDistScale, &width, &height, &xOff, &yOff);

                int xAdvance = 0u;
                stbtt_GetCodepointHMetrics(&prep.fontInfo, codePoint, &xAdvance, NULL);

                range->ptrFontChar[j].xOff = (float)(xOff);
                range->ptrFontChar[j].yOff = (float)(yOff);
                range->ptrFontChar[j].xOff2 = (float)(xOff + width);
                range->ptrFontChar[j].yOff2 = (float)(yOff + height);
                range->ptrFontChar[j].xAdv = prep.scale * (float)xAdvance;

                plFontCustomRect customRect = {
                    .width = (uint32_t)width,
                    .height = (uint32_t)height,
                    .bytes = bytes
                };
                pl_sb_push(atlas->sbCustomRects, customRect);
                prep.area += width * height;
                
            }
            k += prep.ranges[i].num_chars;
        }
        else // regular font
        {
            for(uint32_t j = 0; j < range->charCount; j++)
            {
                int codepoint = 0;
                if(prep.ranges[i].array_of_unicode_codepoints) codepoint = prep.ranges[i].array_of_unicode_codepoints[j];
                else                                           codepoint = prep.ranges[i].first_unicode_codepoint_in_range + j;

                // bitmap
                int glyphIndex = stbtt_FindGlyphIndex(&prep.fontInfo, codepoint);
                if(glyphIndex == 0 && missingGlyphAdded)
                    prep.rects[k].w = prep.rects[k].h = 0;
                else
                {
                    int x0 = 0;
                    int y0 = 0;
                    int x1 = 0;
                    int y1 = 0;
                    stbtt_GetGlyphBitmapBoxSubpixel(&prep.fontInfo, glyphIndex,
                                                    prep.scale * font.config.hOverSampling,
                                                    prep.scale * font.config.vOverSampling,
                                                    0, 0, &x0, &y0, &x1, &y1);
                    prep.rects[k].w = (stbrp_coord)(x1 - x0 + atlas->glyphPadding + font.config.hOverSampling - 1);
                    prep.rects[k].h = (stbrp_coord)(y1 - y0 + atlas->glyphPadding + font.config.vOverSampling - 1);
                    prep.area += prep.rects[k].w * prep.rects[k].h;
                    if (glyphIndex == 0) missingGlyphAdded = true; 
                }
                k++;
            }
        }
        totalCharCount += range->charCount;
    }
    pl_sb_resize(font.sbCodePoints, (uint32_t)maxCodePoint);

    // add font to atlas
    font.parentAtlas = atlas;
    pl_sb_push(atlas->sbFonts, font);
    pl_sb_push(atlas->_sbPrepData, prep);
}

static plVec2
pl__calculate_text_size(plFont* font, float size, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    return pl__calculate_text_size_ex(font, size, text, pcTextEnd, wrap);
}

static plVec2
pl__calculate_text_size_ex(plFont* font, float size, const char* text, const char* pcTextEnd, float wrap)
{
    plVec2 result = {0};
    plVec2 cursor = {0};

    float scale = size > 0.0f ? size / font->config.fontSize : 1.0f;

    float lineSpacing = scale * font->lineSpacing;
    plVec2 originalPosition = {FLT_MAX, FLT_MAX};
    bool firstCharacter = true;

    while(text < pcTextEnd)
    {
        uint32_t c = (uint32_t)*text;
        if(c < 0x80)
            text += 1;
        else
        {
            text += pl_text_char_from_utf8(&c, text, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            cursor.x = originalPosition.x;
            cursor.y += lineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else
        {

            bool glyphFound = false;
            for(uint32_t i = 0u; i < pl_sb_size(font->config.sbRanges); i++)
            {
                if (c >= (uint32_t)font->config.sbRanges[i].firstCodePoint && c < (uint32_t)font->config.sbRanges[i].firstCodePoint + (uint32_t)font->config.sbRanges[i].charCount) 
                {

                    
                    float x0,y0,s0,t0; // top-left
                    float x1,y1,s1,t1; // bottom-right

                    const plFontGlyph* glyph = &font->sbGlyphs[font->sbCodePoints[c]];

                    // adjust for left side bearing if first char
                    if(firstCharacter)
                    {
                        if(glyph->leftBearing > 0.0f) cursor.x += glyph->leftBearing * scale;
                        firstCharacter = false;
                        originalPosition.x = cursor.x + glyph->x0 * scale;
                        originalPosition.y = cursor.y + glyph->y0 * scale;
                    }

                    x0 = cursor.x + glyph->x0 * scale;
                    x1 = cursor.x + glyph->x1 * scale;
                    y0 = cursor.y + glyph->y0 * scale;
                    y1 = cursor.y + glyph->y1 * scale;

                    if(wrap > 0.0f && x1 > originalPosition.x + wrap)
                    {
                        x0 = originalPosition.x + glyph->x0 * scale;
                        y0 = y0 + lineSpacing;
                        x1 = originalPosition.x + glyph->x1 * scale;
                        y1 = y1 + lineSpacing;

                        cursor.x = originalPosition.x;
                        cursor.y += lineSpacing;
                    }

                    if(x0 < originalPosition.x) originalPosition.x = x0;
                    if(y0 < originalPosition.y) originalPosition.y = y0;

                    s0 = glyph->u0;
                    t0 = glyph->v0;
                    s1 = glyph->u1;
                    t1 = glyph->v1;

                    if(x1 > result.x) result.x = x1;
                    if(y1 > result.y) result.y = y1;

                    cursor.x += glyph->xAdvance * scale;
                    glyphFound = true;
                    break;
                }
            }

            PL_ASSERT(glyphFound && "Glyph not found");
        }   
    }

    return pl_sub_vec2(result, originalPosition);
}

static plRect
pl__calculate_text_bb(plFont* ptFont, float fSize, plVec2 tP, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    return pl__calculate_text_bb_ex(ptFont, fSize, tP, pcText, pcTextEnd, fWrap);
}

static plRect
pl__calculate_text_bb_ex(plFont* font, float size, plVec2 tP, const char* text, const char* pcTextEnd, float wrap)
{
    plVec2 tTextSize = {0};
    plVec2 cursor = {0};

    float scale = size > 0.0f ? size / font->config.fontSize : 1.0f;

    float lineSpacing = scale * font->lineSpacing;
    plVec2 originalPosition = {FLT_MAX, FLT_MAX};
    bool firstCharacter = true;

    while(text < pcTextEnd)
    {
        uint32_t c = (uint32_t)*text;
        if(c < 0x80)
            text += 1;
        else
        {
            text += pl_text_char_from_utf8(&c, text, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            cursor.x = originalPosition.x;
            cursor.y += lineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else
        {

            bool glyphFound = false;
            for(uint32_t i = 0u; i < pl_sb_size(font->config.sbRanges); i++)
            {
                if (c >= (uint32_t)font->config.sbRanges[i].firstCodePoint && c < (uint32_t)font->config.sbRanges[i].firstCodePoint + (uint32_t)font->config.sbRanges[i].charCount) 
                {

                    float x0,y0,s0,t0; // top-left
                    float x1,y1,s1,t1; // bottom-right

                    const plFontGlyph* glyph = &font->sbGlyphs[font->sbCodePoints[c]];

                    // adjust for left side bearing if first char
                    if(firstCharacter)
                    {
                        if(glyph->leftBearing > 0.0f) cursor.x += glyph->leftBearing * scale;
                        firstCharacter = false;
                        originalPosition.x = cursor.x + glyph->x0 * scale;
                        originalPosition.y = cursor.y + glyph->y0 * scale;
                    }

                    x0 = cursor.x + glyph->x0 * scale;
                    x1 = cursor.x + glyph->x1 * scale;
                    y0 = cursor.y + glyph->y0 * scale;
                    y1 = cursor.y + glyph->y1 * scale;

                    if(wrap > 0.0f && x1 > originalPosition.x + wrap)
                    {
                        x0 = originalPosition.x + glyph->x0 * scale;
                        y0 = y0 + lineSpacing;
                        x1 = originalPosition.x + glyph->x1 * scale;
                        y1 = y1 + lineSpacing;

                        cursor.x = originalPosition.x;
                        cursor.y += lineSpacing;
                    }

                    if(x0 < originalPosition.x) originalPosition.x = x0;
                    if(y0 < originalPosition.y) originalPosition.y = y0;

                    s0 = glyph->u0;
                    t0 = glyph->v0;
                    s1 = glyph->u1;
                    t1 = glyph->v1;

                    if(x1 > tTextSize.x)
                        tTextSize.x = x1;
                    if(y1 > tTextSize.y)
                        tTextSize.y = y1;

                    cursor.x += glyph->xAdvance * scale;
                    glyphFound = true;
                    break;
                }
            }

            PL_ASSERT(glyphFound && "Glyph not found");
        }   
    }

    tTextSize = pl_sub_vec2(tTextSize, originalPosition);

    const plVec2 tStartOffset = pl_add_vec2(tP, originalPosition);

    const plRect tResult = pl_calculate_rect(tStartOffset, tTextSize);

    return tResult;
}

static void
pl__push_clip_rect_pt(plDrawList* ptDrawlist, const plRect* ptRect)
{
    pl_sb_push(ptDrawlist->sbClipStack, *ptRect);
}

static void
pl__push_clip_rect(plDrawList* ptDrawlist, plRect tRect, bool bAccumulate)
{
    if(bAccumulate && pl_sb_size(ptDrawlist->sbClipStack) > 0)
        tRect = pl_rect_clip_full(&tRect, &pl_sb_back(ptDrawlist->sbClipStack));
    pl_sb_push(ptDrawlist->sbClipStack, tRect);
}

static void
pl__pop_clip_rect(plDrawList* ptDrawlist)
{
    pl_sb_pop(ptDrawlist->sbClipStack);
}

static const plRect*
pl__get_clip_rect(plDrawList* ptDrawlist)
{
     if(pl_sb_size(ptDrawlist->sbClipStack) > 0)
        return &pl_sb_back(ptDrawlist->sbClipStack);
    return NULL;
}

static void
pl__cleanup_draw_context_i(plDrawContext* ctx)
{
    for(uint32_t i = 0u; i < pl_sb_size(ctx->sbDrawlists); i++)
    {
        plDrawList* drawlist = ctx->sbDrawlists[i];
        for(uint32_t j = 0; j < pl_sb_size(drawlist->sbSubmittedLayers); j++)
        {
            pl_sb_free(drawlist->sbSubmittedLayers[j]->sbCommandBuffer);
            pl_sb_free(drawlist->sbSubmittedLayers[j]->sbIndexBuffer);   
            pl_sb_free(drawlist->sbSubmittedLayers[j]->sbPath);  
        }

        for(uint32_t j = 0; j < pl_sb_size(drawlist->sbLayersCreated); j++)
        {
            PL_FREE(drawlist->sbLayersCreated[j]);
        }
        pl_sb_free(drawlist->sbDrawCommands);
        pl_sb_free(drawlist->sbVertexBuffer);
        pl_sb_free(drawlist->sbLayerCache);
        pl_sb_free(drawlist->sbLayersCreated);
        pl_sb_free(drawlist->sbSubmittedLayers);   
        pl_sb_free(drawlist->sbClipStack);
    }
    for(uint32_t i = 0u; i < pl_sb_size(ctx->sb3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ctx->sb3DDrawlists[i];
        pl_sb_free(drawlist->sbIndexBuffer);
        pl_sb_free(drawlist->sbVertexBuffer);
        pl_sb_free(drawlist->sbLineVertexBuffer);
        pl_sb_free(drawlist->sbLineIndexBuffer);  
    }
    pl_sb_free(ctx->sbDrawlists);
    pl_sb_free(ctx->sb3DDrawlists);
}

static void
pl__build_font_atlas_i(plFontAtlas* atlas)
{
    // calculate texture total area needed
    uint32_t totalAtlasArea = 0u;
    for(uint32_t i = 0u; i < pl_sb_size(atlas->_sbPrepData); i++)
        totalAtlasArea += atlas->_sbPrepData[i].area;

    // create our white location
    plFontCustomRect whiteRect = {
        .width = 8u,
        .height = 8u,
        .x = 0u,
        .y = 0u,
        .bytes = PL_ALLOC(64)
    };
    memset(whiteRect.bytes, 255, 64);
    pl_sb_push(atlas->sbCustomRects, whiteRect);
    atlas->whiteRect = &pl_sb_back(atlas->sbCustomRects);
    totalAtlasArea += 64;

    // calculate final texture area required
    const float totalAtlasAreaSqrt = (float)sqrt((float)totalAtlasArea) + 1.0f;
    atlas->atlasSize[0] = 512;
    atlas->atlasSize[1] = 0;
    if     (totalAtlasAreaSqrt >= 4096 * 0.7f) atlas->atlasSize[0] = 4096;
    else if(totalAtlasAreaSqrt >= 2048 * 0.7f) atlas->atlasSize[0] = 2048;
    else if(totalAtlasAreaSqrt >= 1024 * 0.7f) atlas->atlasSize[0] = 1024;

    // begin packing
    stbtt_pack_context spc = {0};
    stbtt_PackBegin(&spc, NULL, atlas->atlasSize[0], 1024 * 32, 0, atlas->glyphPadding, NULL);

    // allocate SDF rects
    stbrp_rect* rects = PL_ALLOC(pl_sb_size(atlas->sbCustomRects) * sizeof(stbrp_rect));
    memset(rects, 0, sizeof(stbrp_rect) * pl_sb_size(atlas->sbCustomRects));

    // transfer our data to stb data
    for(uint32_t i = 0u; i < pl_sb_size(atlas->sbCustomRects); i++)
    {
        rects[i].w = (int)atlas->sbCustomRects[i].width;
        rects[i].h = (int)atlas->sbCustomRects[i].height;
    }
    
    // pack bitmap fonts
    for(uint32_t i = 0u; i < pl_sb_size(atlas->_sbPrepData); i++)
    {
        plFont* font = &atlas->sbFonts[i];
        if(!font->config.sdf)
        {
            plFontPrepData* prep = &atlas->_sbPrepData[i];
            stbtt_PackSetOversampling(&spc, font->config.hOverSampling, font->config.vOverSampling);
            stbrp_pack_rects((stbrp_context*)spc.pack_info, prep->rects, prep->uTotalCharCount);
            for(uint32_t j = 0u; j < prep->uTotalCharCount; j++)
            {
                if(prep->rects[j].was_packed)
                    atlas->atlasSize[1] = (uint32_t)pl__get_max((float)atlas->atlasSize[1], (float)(prep->rects[j].y + prep->rects[j].h));
            }
        }
    }

    // pack SDF fonts
    stbtt_PackSetOversampling(&spc, 1, 1);
    stbrp_pack_rects((stbrp_context*)spc.pack_info, rects, pl_sb_size(atlas->sbCustomRects));

    for(uint32_t i = 0u; i < pl_sb_size(atlas->sbCustomRects); i++)
    {
        if(rects[i].was_packed)
            atlas->atlasSize[1] = (uint32_t)pl__get_max((float)atlas->atlasSize[1], (float)(rects[i].y + rects[i].h));
    }

    // grow cpu side buffers if needed
    if(atlas->pixelDataSize < atlas->atlasSize[0] * atlas->atlasSize[1])
    {
        if(atlas->pixelsAsAlpha8) PL_FREE(atlas->pixelsAsAlpha8);
        if(atlas->pixelsAsRGBA32) PL_FREE(atlas->pixelsAsRGBA32);

        atlas->pixelsAsAlpha8 = PL_ALLOC(atlas->atlasSize[0] * atlas->atlasSize[1]);   
        atlas->pixelsAsRGBA32 = PL_ALLOC(atlas->atlasSize[0] * atlas->atlasSize[1] * 4);

        memset(atlas->pixelsAsAlpha8, 0, atlas->atlasSize[0] * atlas->atlasSize[1]);
        memset(atlas->pixelsAsRGBA32, 0, atlas->atlasSize[0] * atlas->atlasSize[1] * 4);
    }
    spc.pixels = atlas->pixelsAsAlpha8;
    atlas->pixelDataSize = atlas->atlasSize[0] * atlas->atlasSize[1];

    // rasterize bitmap fonts
    for(uint32_t i = 0u; i < pl_sb_size(atlas->_sbPrepData); i++)
    {
        plFont* font = &atlas->sbFonts[i];
        plFontPrepData* prep = &atlas->_sbPrepData[i];
        if(!atlas->sbFonts[i].config.sdf)
            stbtt_PackFontRangesRenderIntoRects(&spc, &prep->fontInfo, prep->ranges, pl_sb_size(font->config.sbRanges), prep->rects);
    }

    // update SDF/custom data
    for(uint32_t i = 0u; i < pl_sb_size(atlas->sbCustomRects); i++)
    {
        atlas->sbCustomRects[i].x = (uint32_t)rects[i].x;
        atlas->sbCustomRects[i].y = (uint32_t)rects[i].y;
    }

    uint32_t charDataOffset = 0u;
    for(uint32_t fontIndex = 0u; fontIndex < pl_sb_size(atlas->sbFonts); fontIndex++)
    {
        plFont* font = &atlas->sbFonts[fontIndex];
        if(font->config.sdf)
        {
            for(uint32_t i = 0u; i < pl_sb_size(font->sbCharData); i++)
            {
                font->sbCharData[i].x0 = (uint16_t)rects[charDataOffset + i].x;
                font->sbCharData[i].y0 = (uint16_t)rects[charDataOffset + i].y;
                font->sbCharData[i].x1 = (uint16_t)(rects[charDataOffset + i].x + atlas->sbCustomRects[charDataOffset + i].width);
                font->sbCharData[i].y1 = (uint16_t)(rects[charDataOffset + i].y + atlas->sbCustomRects[charDataOffset + i].height);
                
            }
            charDataOffset += pl_sb_size(font->sbCharData);  
        }
    }

    // end packing
    stbtt_PackEnd(&spc);

    // rasterize SDF/custom rects
    for(uint32_t r = 0u; r < pl_sb_size(atlas->sbCustomRects); r++)
    {
        plFontCustomRect* customRect = &atlas->sbCustomRects[r];
        for(uint32_t i = 0u; i < customRect->height; i++)
        {
            for(uint32_t j = 0u; j < customRect->width; j++)
                atlas->pixelsAsAlpha8[(customRect->y + i) * atlas->atlasSize[0] + (customRect->x + j)] =  customRect->bytes[i * customRect->width + j];
        }
        stbtt_FreeSDF(customRect->bytes, NULL);
        customRect->bytes = NULL;
    }

    // update white point uvs
    atlas->whiteUv[0] = (float)(atlas->whiteRect->x + atlas->whiteRect->width / 2) / (float)atlas->atlasSize[0];
    atlas->whiteUv[1] = (float)(atlas->whiteRect->y + atlas->whiteRect->height / 2) / (float)atlas->atlasSize[1];

    // add glyphs
    for(uint32_t fontIndex = 0u; fontIndex < pl_sb_size(atlas->sbFonts); fontIndex++)
    {
        plFont* font = &atlas->sbFonts[fontIndex];

        uint32_t charIndex = 0u;
        float pixelHeight = 0.0f;
        if(font->config.sdf) pixelHeight = 0.5f * 1.0f / (float)atlas->atlasSize[1]; // is this correct?
        
        for(uint32_t i = 0u; i < pl_sb_size(font->config.sbRanges); i++)
        {
            plFontRange* range = &font->config.sbRanges[i];
            for(uint32_t j = 0u; j < range->charCount; j++)
            {
                const int codePoint = range->firstCodePoint + j;
                stbtt_aligned_quad q;
                float unused_x = 0.0f, unused_y = 0.0f;
                stbtt_GetPackedQuad((stbtt_packedchar*)font->sbCharData, atlas->atlasSize[0], atlas->atlasSize[1], charIndex, &unused_x, &unused_y, &q, 0);

                int unusedAdvanced, leftSideBearing;
                stbtt_GetCodepointHMetrics(&atlas->_sbPrepData[fontIndex].fontInfo, codePoint, &unusedAdvanced, &leftSideBearing);

                plFontGlyph glyph = {
                    .x0 = q.x0,
                    .y0 = q.y0 + font->ascent,
                    .x1 = q.x1,
                    .y1 = q.y1 + font->ascent,
                    .u0 = q.s0,
                    .v0 = q.t0 + pixelHeight,
                    .u1 = q.s1,
                    .v1 = q.t1 - pixelHeight,
                    .xAdvance = font->sbCharData[charIndex].xAdv,
                    .leftBearing = (float)leftSideBearing * atlas->_sbPrepData[fontIndex].scale
                };
                pl_sb_push(font->sbGlyphs, glyph);
                font->sbCodePoints[codePoint] = pl_sb_size(font->sbGlyphs) - 1;
                charIndex++;
            }
        }

        PL_FREE(atlas->_sbPrepData[fontIndex].fontInfo.data);
    }

    // convert to 4 color channels
    for(uint32_t i = 0u; i < atlas->atlasSize[0] * atlas->atlasSize[1]; i++)
    {
        atlas->pixelsAsRGBA32[i * 4] = 255;
        atlas->pixelsAsRGBA32[i * 4 + 1] = 255;
        atlas->pixelsAsRGBA32[i * 4 + 2] = 255;
        atlas->pixelsAsRGBA32[i * 4 + 3] = atlas->pixelsAsAlpha8[i];
    }

    PL_FREE(rects);
}

static void
pl__cleanup_font_atlas_i(plFontAtlas* atlas)
{
    for(uint32_t i = 0; i < pl_sb_size(atlas->sbFonts); i++)
    {
        plFont* font = &atlas->sbFonts[i];
        pl_sb_free(font->config.sbRanges);
        pl_sb_free(font->config.sbIndividualChars);
        pl_sb_free(font->sbCodePoints);
        pl_sb_free(font->sbGlyphs);
        pl_sb_free(font->sbCharData);
    }
    for(uint32_t i = 0; i < pl_sb_size(atlas->_sbPrepData); i++)
    {
        PL_FREE(atlas->_sbPrepData[i].ranges);
        PL_FREE(atlas->_sbPrepData[i].rects);
    }
    for(uint32_t i = 0; i < pl_sb_size(atlas->sbCustomRects); i++)
    {
        PL_FREE(atlas->sbCustomRects[i].bytes);
    }
    pl_sb_free(atlas->sbCustomRects);
    pl_sb_free(atlas->sbFonts);
    pl_sb_free(atlas->_sbPrepData);
    PL_FREE(atlas->pixelsAsAlpha8);
    PL_FREE(atlas->pixelsAsRGBA32);
}

static void
pl__prepare_draw_command(plDrawLayer* layer, plTextureId textureID, bool sdf)
{
    bool createNewCommand = true;

    const plRect tCurrentClip = pl_sb_size(layer->drawlist->sbClipStack) > 0 ? pl_sb_top(layer->drawlist->sbClipStack) : (plRect){0};

    
    if(layer->_lastCommand)
    {
        // check if last command has same texture
        if(layer->_lastCommand->textureId == textureID && layer->_lastCommand->sdf == sdf)
        {
            createNewCommand = false;
        }

        // check if last command has same clipping
        if(layer->_lastCommand->tClip.tMax.x != tCurrentClip.tMax.x || layer->_lastCommand->tClip.tMax.y != tCurrentClip.tMax.y ||
            layer->_lastCommand->tClip.tMin.x != tCurrentClip.tMin.x || layer->_lastCommand->tClip.tMin.y != tCurrentClip.tMin.y)
        {
            createNewCommand = true;
        }
    }

    // new command needed
    if(createNewCommand)
    {
        plDrawCommand newdrawCommand = 
        {
            .vertexOffset = pl_sb_size(layer->drawlist->sbVertexBuffer),
            .indexOffset  = pl_sb_size(layer->sbIndexBuffer),
            .elementCount = 0u,
            .textureId    = textureID,
            .sdf          = sdf,
            .tClip        = tCurrentClip
        };
        pl_sb_push(layer->sbCommandBuffer, newdrawCommand);
        
    }
    layer->_lastCommand = &pl_sb_top(layer->sbCommandBuffer);
    layer->_lastCommand->textureId = textureID;
}

static void
pl__reserve_triangles(plDrawLayer* layer, uint32_t indexCount, uint32_t vertexCount)
{
    pl_sb_reserve(layer->drawlist->sbVertexBuffer, pl_sb_size(layer->drawlist->sbVertexBuffer) + vertexCount);
    pl_sb_reserve(layer->sbIndexBuffer, pl_sb_size(layer->sbIndexBuffer) + indexCount);
    layer->_lastCommand->elementCount += indexCount; 
    layer->vertexCount += vertexCount;
}

static void
pl__add_vertex(plDrawLayer* layer, plVec2 pos, plVec4 color, plVec2 uv)
{

    uint32_t tcolor = 0;
    tcolor = (uint32_t)  (255.0f * color.r + 0.5f);
    tcolor |= (uint32_t) (255.0f * color.g + 0.5f) << 8;
    tcolor |= (uint32_t) (255.0f * color.b + 0.5f) << 16;
    tcolor |= (uint32_t) (255.0f * color.a + 0.5f) << 24;

    pl_sb_push(layer->drawlist->sbVertexBuffer,
        ((plDrawVertex){
            .pos[0] = pos.x,
            .pos[1] = pos.y,
            .uv[0] = uv.u,
            .uv[1] = uv.v,
            .uColor = tcolor
        })
    );
}

static void
pl__add_index(plDrawLayer* layer, uint32_t vertexStart, uint32_t i0, uint32_t i1, uint32_t i2)
{
    pl_sb_push(layer->sbIndexBuffer, vertexStart + i0);
    pl_sb_push(layer->sbIndexBuffer, vertexStart + i1);
    pl_sb_push(layer->sbIndexBuffer, vertexStart + i2);
}

static char*
pl__read_file(const char* file)
{
    FILE* fileHandle = fopen(file, "rb");

    if(fileHandle == NULL)
    {
        PL_ASSERT(false && "TTF file not found.");
        return NULL;
    }

    // obtain file size
    fseek(fileHandle, 0, SEEK_END);
    uint32_t fileSize = ftell(fileHandle);
    fseek(fileHandle, 0, SEEK_SET);

    // allocate buffer
    char* data = PL_ALLOC(fileSize);

    // copy file into buffer
    size_t result = fread(data, sizeof(char), fileSize, fileHandle);
    if(result != fileSize)
    {
        if(feof(fileHandle))
        {
            PL_ASSERT(false && "Error reading TTF file: unexpected end of file");
        }
        else if (ferror(fileHandle))
        {
            PL_ASSERT(false && "Error reading TTF file.");
        }
        PL_ASSERT(false && "TTF file not read.");
    }

    fclose(fileHandle);
    return data;
}

//-----------------------------------------------------------------------------
// [SECTION] default font stuff
//-----------------------------------------------------------------------------

static uint32_t
pl__decompress_length(const unsigned char* ptrInput)
{
    if(ptrInput)
        return (ptrInput[8] << 24) + (ptrInput[9] << 16) + (ptrInput[10] << 8) + ptrInput[11];
    return 0u;
}

static uint32_t
pl__decode85_byte(char c) { return (c >= '\\') ? c - 36 : c -35;}

static void
pl__decode85(const unsigned char* ptrSrc, unsigned char* ptrDst)
{
    while (*ptrSrc)
    {
        uint32_t uTmp = pl__decode85_byte(ptrSrc[0]) + 85 * (pl__decode85_byte(ptrSrc[1]) + 85 * (pl__decode85_byte(ptrSrc[2]) + 85 * (pl__decode85_byte(ptrSrc[3]) + 85 * pl__decode85_byte(ptrSrc[4]))));
        ptrDst[0] = ((uTmp >> 0) & 0xFF); 
        ptrDst[1] = ((uTmp >> 8) & 0xFF); 
        ptrDst[2] = ((uTmp >> 16) & 0xFF); 
        ptrDst[3] = ((uTmp >> 24) & 0xFF);
        ptrSrc += 5;
        ptrDst += 4;
    }
}

static unsigned char* ptrBarrierOutE_ = NULL;
static unsigned char* ptrBarrierOutB_ = NULL;
static const unsigned char * ptrBarrierInB_;
static unsigned char* ptrDOut_ = NULL;

static void 
pl__match(const unsigned char* ptrData, uint32_t uLength)
{
    PL_ASSERT(ptrDOut_ + uLength <= ptrBarrierOutE_);
    if (ptrDOut_ + uLength > ptrBarrierOutE_) ptrDOut_ += uLength; 
    else if (ptrData < ptrBarrierOutB_)       ptrDOut_ = ptrBarrierOutE_ + 1;
    else while (uLength--) *ptrDOut_++ = *ptrData++;
}

static void 
pl__lit(const unsigned char* ptrData, uint32_t uLength)
{
    PL_ASSERT(ptrDOut_ + uLength <= ptrBarrierOutE_);
    if (ptrDOut_ + uLength > ptrBarrierOutE_) ptrDOut_ += uLength;
    else if (ptrData < ptrBarrierInB_)        ptrDOut_ = ptrBarrierOutE_ + 1; 
    else { memcpy(ptrDOut_, ptrData, uLength); ptrDOut_ += uLength; }
}

#define MV_IN2_(x) ((ptrI[x] << 8) + ptrI[(x) + 1])
#define MV_IN3_(x) ((ptrI[x] << 16) + MV_IN2_((x) + 1))
#define MV_IN4_(x) ((ptrI[x] << 24) + MV_IN3_((x) + 1))

static const unsigned char*
pl__decompress_token(const unsigned char* ptrI)
{
    if (*ptrI >= 0x20) 
    {
        if      (*ptrI >= 0x80)  pl__match(ptrDOut_ - ptrI[1] - 1, ptrI[0] - 0x80 + 1), ptrI += 2;
        else if (*ptrI >= 0x40)  pl__match(ptrDOut_ - (MV_IN2_(0) - 0x4000 + 1), ptrI[2] + 1), ptrI += 3;
        else /* *ptrI >= 0x20 */ pl__lit(ptrI + 1, ptrI[0] - 0x20 + 1), ptrI += 1 + (ptrI[0] - 0x20 + 1);
    } 
    else 
    {
        if      (*ptrI >= 0x18) pl__match(ptrDOut_ - (MV_IN3_(0) - 0x180000 + 1), ptrI[3] + 1), ptrI += 4;
        else if (*ptrI >= 0x10) pl__match(ptrDOut_ - (MV_IN3_(0) - 0x100000 + 1), MV_IN2_(3) + 1), ptrI += 5;
        else if (*ptrI >= 0x08) pl__lit(ptrI + 2, MV_IN2_(0) - 0x0800 + 1), ptrI += 2 + (MV_IN2_(0) - 0x0800 + 1);
        else if (*ptrI == 0x07) pl__lit(ptrI + 3, MV_IN2_(1) + 1), ptrI += 3 + (MV_IN2_(1) + 1);
        else if (*ptrI == 0x06) pl__match(ptrDOut_ - (MV_IN3_(1) + 1), ptrI[4] + 1), ptrI += 5;
        else if (*ptrI == 0x04) pl__match(ptrDOut_ - (MV_IN3_(1) + 1), MV_IN2_(4) + 1), ptrI += 6;
    }
    return ptrI;
}

static uint32_t 
pl__adler32(uint32_t uAdler32, unsigned char* ptrBuf, uint32_t uBufLen)
{
    const uint32_t ADLER_MOD = 65521;
    uint32_t s1 = uAdler32 & 0xffff;
    uint32_t s2 = uAdler32 >> 16;
    uint32_t blocklen = uBufLen % 5552;

    uint32_t i = 0;
    while (uBufLen) 
    {
        for (i = 0; i + 7 < blocklen; i += 8) 
        {
            s1 += ptrBuf[0], s2 += s1;
            s1 += ptrBuf[1], s2 += s1;
            s1 += ptrBuf[2], s2 += s1;
            s1 += ptrBuf[3], s2 += s1;
            s1 += ptrBuf[4], s2 += s1;
            s1 += ptrBuf[5], s2 += s1;
            s1 += ptrBuf[6], s2 += s1;
            s1 += ptrBuf[7], s2 += s1;
            ptrBuf += 8;
        }

        for (; i < blocklen; ++i)
            s1 += *ptrBuf++, s2 += s1;

        s1 %= ADLER_MOD;
        s2 %= ADLER_MOD;
        uBufLen -= blocklen;
        blocklen = 5552;
    }
    return (uint32_t)(s2 << 16) + (uint32_t)s1;
}

static uint32_t 
pl__decompress(unsigned char* ptrOut, const unsigned char* ptrI, uint32_t length)
{
    if(ptrI == NULL)
        return 0u;
    if (MV_IN4_(0) != 0x57bC0000) 
        return 0u;
    if (MV_IN4_(4) != 0)
        return 0u;

    const uint32_t uOLen = pl__decompress_length(ptrI);
    ptrBarrierInB_ = ptrI;
    ptrBarrierOutE_ = ptrOut + uOLen;
    ptrBarrierOutB_ = ptrOut;
    ptrI += 16;

    ptrDOut_ = ptrOut;
    for (;;) 
    {
        const unsigned char* ptrOldI = ptrI;
        ptrI = pl__decompress_token(ptrI);
        if (ptrI == ptrOldI) 
        {
            if (*ptrI == 0x05 && ptrI[1] == 0xfa) 
            {
                PL_ASSERT(ptrDOut_ == ptrOut + uOLen);
                if (ptrDOut_ != ptrOut + uOLen) break;
                if (pl__adler32(1, ptrOut, uOLen) != (uint32_t) MV_IN4_(2))break;
                return uOLen;
            } 
        }
        PL_ASSERT(ptrDOut_ <= ptrOut + uOLen);
        if (ptrDOut_ > ptrOut + uOLen)
            break;
    }
    return 0u;
}

// File: 'ProggyClean.ttf' (41208 bytes)
static const char gcPtrDefaultFontCompressed[11980 + 1] =
    "7])#######hV0qs'/###[),##/l:$#Q6>##5[n42>c-TH`->>#/e>11NNV=Bv(*:.F?uu#(gRU.o0XGH`$vhLG1hxt9?W`#,5LsCp#-i>.r$<$6pD>Lb';9Crc6tgXmKVeU2cD4Eo3R/"
    "2*>]b(MC;$jPfY.;h^`IWM9<Lh2TlS+f-s$o6Q<BWH`YiU.xfLq$N;$0iR/GX:U(jcW2p/W*q?-qmnUCI;jHSAiFWM.R*kU@C=GH?a9wp8f$e.-4^Qg1)Q-GL(lf(r/7GrRgwV%MS=C#"
    "`8ND>Qo#t'X#(v#Y9w0#1D$CIf;W'#pWUPXOuxXuU(H9M(1<q-UE31#^-V'8IRUo7Qf./L>=Ke$$'5F%)]0^#0X@U.a<r:QLtFsLcL6##lOj)#.Y5<-R&KgLwqJfLgN&;Q?gI^#DY2uL"
    "i@^rMl9t=cWq6##weg>$FBjVQTSDgEKnIS7EM9>ZY9w0#L;>>#Mx&4Mvt//L[MkA#W@lK.N'[0#7RL_&#w+F%HtG9M#XL`N&.,GM4Pg;-<nLENhvx>-VsM.M0rJfLH2eTM`*oJMHRC`N"
    "kfimM2J,W-jXS:)r0wK#@Fge$U>`w'N7G#$#fB#$E^$#:9:hk+eOe--6x)F7*E%?76%^GMHePW-Z5l'&GiF#$956:rS?dA#fiK:)Yr+`&#0j@'DbG&#^$PG.Ll+DNa<XCMKEV*N)LN/N"
    "*b=%Q6pia-Xg8I$<MR&,VdJe$<(7G;Ckl'&hF;;$<_=X(b.RS%%)###MPBuuE1V:v&cX&#2m#(&cV]`k9OhLMbn%s$G2,B$BfD3X*sp5#l,$R#]x_X1xKX%b5U*[r5iMfUo9U`N99hG)"
    "tm+/Us9pG)XPu`<0s-)WTt(gCRxIg(%6sfh=ktMKn3j)<6<b5Sk_/0(^]AaN#(p/L>&VZ>1i%h1S9u5o@YaaW$e+b<TWFn/Z:Oh(Cx2$lNEoN^e)#CFY@@I;BOQ*sRwZtZxRcU7uW6CX"
    "ow0i(?$Q[cjOd[P4d)]>ROPOpxTO7Stwi1::iB1q)C_=dV26J;2,]7op$]uQr@_V7$q^%lQwtuHY]=DX,n3L#0PHDO4f9>dC@O>HBuKPpP*E,N+b3L#lpR/MrTEH.IAQk.a>D[.e;mc."
    "x]Ip.PH^'/aqUO/$1WxLoW0[iLA<QT;5HKD+@qQ'NQ(3_PLhE48R.qAPSwQ0/WK?Z,[x?-J;jQTWA0X@KJ(_Y8N-:/M74:/-ZpKrUss?d#dZq]DAbkU*JqkL+nwX@@47`5>w=4h(9.`G"
    "CRUxHPeR`5Mjol(dUWxZa(>STrPkrJiWx`5U7F#.g*jrohGg`cg:lSTvEY/EV_7H4Q9[Z%cnv;JQYZ5q.l7Zeas:HOIZOB?G<Nald$qs]@]L<J7bR*>gv:[7MI2k).'2($5FNP&EQ(,)"
    "U]W]+fh18.vsai00);D3@4ku5P?DP8aJt+;qUM]=+b'8@;mViBKx0DE[-auGl8:PJ&Dj+M6OC]O^((##]`0i)drT;-7X`=-H3[igUnPG-NZlo.#k@h#=Ork$m>a>$-?Tm$UV(?#P6YY#"
    "'/###xe7q.73rI3*pP/$1>s9)W,JrM7SN]'/4C#v$U`0#V.[0>xQsH$fEmPMgY2u7Kh(G%siIfLSoS+MK2eTM$=5,M8p`A.;_R%#u[K#$x4AG8.kK/HSB==-'Ie/QTtG?-.*^N-4B/ZM"
    "_3YlQC7(p7q)&](`6_c)$/*JL(L-^(]$wIM`dPtOdGA,U3:w2M-0<q-]L_?^)1vw'.,MRsqVr.L;aN&#/EgJ)PBc[-f>+WomX2u7lqM2iEumMTcsF?-aT=Z-97UEnXglEn1K-bnEO`gu"
    "Ft(c%=;Am_Qs@jLooI&NX;]0#j4#F14;gl8-GQpgwhrq8'=l_f-b49'UOqkLu7-##oDY2L(te+Mch&gLYtJ,MEtJfLh'x'M=$CS-ZZ%P]8bZ>#S?YY#%Q&q'3^Fw&?D)UDNrocM3A76/"
    "/oL?#h7gl85[qW/NDOk%16ij;+:1a'iNIdb-ou8.P*w,v5#EI$TWS>Pot-R*H'-SEpA:g)f+O$%%`kA#G=8RMmG1&O`>to8bC]T&$,n.LoO>29sp3dt-52U%VM#q7'DHpg+#Z9%H[K<L"
    "%a2E-grWVM3@2=-k22tL]4$##6We'8UJCKE[d_=%wI;'6X-GsLX4j^SgJ$##R*w,vP3wK#iiW&#*h^D&R?jp7+/u&#(AP##XU8c$fSYW-J95_-Dp[g9wcO&#M-h1OcJlc-*vpw0xUX&#"
    "OQFKNX@QI'IoPp7nb,QU//MQ&ZDkKP)X<WSVL(68uVl&#c'[0#(s1X&xm$Y%B7*K:eDA323j998GXbA#pwMs-jgD$9QISB-A_(aN4xoFM^@C58D0+Q+q3n0#3U1InDjF682-SjMXJK)("
    "h$hxua_K]ul92%'BOU&#BRRh-slg8KDlr:%L71Ka:.A;%YULjDPmL<LYs8i#XwJOYaKPKc1h:'9Ke,g)b),78=I39B;xiY$bgGw-&.Zi9InXDuYa%G*f2Bq7mn9^#p1vv%#(Wi-;/Z5h"
    "o;#2:;%d&#x9v68C5g?ntX0X)pT`;%pB3q7mgGN)3%(P8nTd5L7GeA-GL@+%J3u2:(Yf>et`e;)f#Km8&+DC$I46>#Kr]]u-[=99tts1.qb#q72g1WJO81q+eN'03'eM>&1XxY-caEnO"
    "j%2n8)),?ILR5^.Ibn<-X-Mq7[a82Lq:F&#ce+S9wsCK*x`569E8ew'He]h:sI[2LM$[guka3ZRd6:t%IG:;$%YiJ:Nq=?eAw;/:nnDq0(CYcMpG)qLN4$##&J<j$UpK<Q4a1]MupW^-"
    "sj_$%[HK%'F####QRZJ::Y3EGl4'@%FkiAOg#p[##O`gukTfBHagL<LHw%q&OV0##F=6/:chIm0@eCP8X]:kFI%hl8hgO@RcBhS-@Qb$%+m=hPDLg*%K8ln(wcf3/'DW-$.lR?n[nCH-"
    "eXOONTJlh:.RYF%3'p6sq:UIMA945&^HFS87@$EP2iG<-lCO$%c`uKGD3rC$x0BL8aFn--`ke%#HMP'vh1/R&O_J9'um,.<tx[@%wsJk&bUT2`0uMv7gg#qp/ij.L56'hl;.s5CUrxjO"
    "M7-##.l+Au'A&O:-T72L]P`&=;ctp'XScX*rU.>-XTt,%OVU4)S1+R-#dg0/Nn?Ku1^0f$B*P:Rowwm-`0PKjYDDM'3]d39VZHEl4,.j']Pk-M.h^&:0FACm$maq-&sgw0t7/6(^xtk%"
    "LuH88Fj-ekm>GA#_>568x6(OFRl-IZp`&b,_P'$M<Jnq79VsJW/mWS*PUiq76;]/NM_>hLbxfc$mj`,O;&%W2m`Zh:/)Uetw:aJ%]K9h:TcF]u_-Sj9,VK3M.*'&0D[Ca]J9gp8,kAW]"
    "%(?A%R$f<->Zts'^kn=-^@c4%-pY6qI%J%1IGxfLU9CP8cbPlXv);C=b),<2mOvP8up,UVf3839acAWAW-W?#ao/^#%KYo8fRULNd2.>%m]UK:n%r$'sw]J;5pAoO_#2mO3n,'=H5(et"
    "Hg*`+RLgv>=4U8guD$I%D:W>-r5V*%j*W:Kvej.Lp$<M-SGZ':+Q_k+uvOSLiEo(<aD/K<CCc`'Lx>'?;++O'>()jLR-^u68PHm8ZFWe+ej8h:9r6L*0//c&iH&R8pRbA#Kjm%upV1g:"
    "a_#Ur7FuA#(tRh#.Y5K+@?3<-8m0$PEn;J:rh6?I6uG<-`wMU'ircp0LaE_OtlMb&1#6T.#FDKu#1Lw%u%+GM+X'e?YLfjM[VO0MbuFp7;>Q&#WIo)0@F%q7c#4XAXN-U&VB<HFF*qL("
    "$/V,;(kXZejWO`<[5?\?ewY(*9=%wDc;,u<'9t3W-(H1th3+G]ucQ]kLs7df($/*JL]@*t7Bu_G3_7mp7<iaQjO@.kLg;x3B0lqp7Hf,^Ze7-##@/c58Mo(3;knp0%)A7?-W+eI'o8)b<"
    "nKnw'Ho8C=Y>pqB>0ie&jhZ[?iLR@@_AvA-iQC(=ksRZRVp7`.=+NpBC%rh&3]R:8XDmE5^V8O(x<<aG/1N$#FX$0V5Y6x'aErI3I$7x%E`v<-BY,)%-?Psf*l?%C3.mM(=/M0:JxG'?"
    "7WhH%o'a<-80g0NBxoO(GH<dM]n.+%q@jH?f.UsJ2Ggs&4<-e47&Kl+f//9@`b+?.TeN_&B8Ss?v;^Trk;f#YvJkl&w$]>-+k?'(<S:68tq*WoDfZu';mM?8X[ma8W%*`-=;D.(nc7/;"
    ")g:T1=^J$&BRV(-lTmNB6xqB[@0*o.erM*<SWF]u2=st-*(6v>^](H.aREZSi,#1:[IXaZFOm<-ui#qUq2$##Ri;u75OK#(RtaW-K-F`S+cF]uN`-KMQ%rP/Xri.LRcB##=YL3BgM/3M"
    "D?@f&1'BW-)Ju<L25gl8uhVm1hL$##*8###'A3/LkKW+(^rWX?5W_8g)a(m&K8P>#bmmWCMkk&#TR`C,5d>g)F;t,4:@_l8G/5h4vUd%&%950:VXD'QdWoY-F$BtUwmfe$YqL'8(PWX("
    "P?^@Po3$##`MSs?DWBZ/S>+4%>fX,VWv/w'KD`LP5IbH;rTV>n3cEK8U#bX]l-/V+^lj3;vlMb&[5YQ8#pekX9JP3XUC72L,,?+Ni&co7ApnO*5NK,((W-i:$,kp'UDAO(G0Sq7MVjJs"
    "bIu)'Z,*[>br5fX^:FPAWr-m2KgL<LUN098kTF&#lvo58=/vjDo;.;)Ka*hLR#/k=rKbxuV`>Q_nN6'8uTG&#1T5g)uLv:873UpTLgH+#FgpH'_o1780Ph8KmxQJ8#H72L4@768@Tm&Q"
    "h4CB/5OvmA&,Q&QbUoi$a_%3M01H)4x7I^&KQVgtFnV+;[Pc>[m4k//,]1?#`VY[Jr*3&&slRfLiVZJ:]?=K3Sw=[$=uRB?3xk48@aeg<Z'<$#4H)6,>e0jT6'N#(q%.O=?2S]u*(m<-"
    "V8J'(1)G][68hW$5'q[GC&5j`TE?m'esFGNRM)j,ffZ?-qx8;->g4t*:CIP/[Qap7/9'#(1sao7w-.qNUdkJ)tCF&#B^;xGvn2r9FEPFFFcL@.iFNkTve$m%#QvQS8U@)2Z+3K:AKM5i"
    "sZ88+dKQ)W6>J%CL<KE>`.d*(B`-n8D9oK<Up]c$X$(,)M8Zt7/[rdkqTgl-0cuGMv'?>-XV1q['-5k'cAZ69e;D_?$ZPP&s^+7])$*$#@QYi9,5P&#9r+$%CE=68>K8r0=dSC%%(@p7"
    ".m7jilQ02'0-VWAg<a/''3u.=4L$Y)6k/K:_[3=&jvL<L0C/2'v:^;-DIBW,B4E68:kZ;%?8(Q8BH=kO65BW?xSG&#@uU,DS*,?.+(o(#1vCS8#CHF>TlGW'b)Tq7VT9q^*^$$.:&N@@"
    "$&)WHtPm*5_rO0&e%K&#-30j(E4#'Zb.o/(Tpm$>K'f@[PvFl,hfINTNU6u'0pao7%XUp9]5.>%h`8_=VYbxuel.NTSsJfLacFu3B'lQSu/m6-Oqem8T+oE--$0a/k]uj9EwsG>%veR*"
    "hv^BFpQj:K'#SJ,sB-'#](j.Lg92rTw-*n%@/;39rrJF,l#qV%OrtBeC6/,;qB3ebNW[?,Hqj2L.1NP&GjUR=1D8QaS3Up&@*9wP?+lo7b?@%'k4`p0Z$22%K3+iCZj?XJN4Nm&+YF]u"
    "@-W$U%VEQ/,,>>#)D<h#`)h0:<Q6909ua+&VU%n2:cG3FJ-%@Bj-DgLr`Hw&HAKjKjseK</xKT*)B,N9X3]krc12t'pgTV(Lv-tL[xg_%=M_q7a^x?7Ubd>#%8cY#YZ?=,`Wdxu/ae&#"
    "w6)R89tI#6@s'(6Bf7a&?S=^ZI_kS&ai`&=tE72L_D,;^R)7[$s<Eh#c&)q.MXI%#v9ROa5FZO%sF7q7Nwb&#ptUJ:aqJe$Sl68%.D###EC><?-aF&#RNQv>o8lKN%5/$(vdfq7+ebA#"
    "u1p]ovUKW&Y%q]'>$1@-[xfn$7ZTp7mM,G,Ko7a&Gu%G[RMxJs[0MM%wci.LFDK)(<c`Q8N)jEIF*+?P2a8g%)$q]o2aH8C&<SibC/q,(e:v;-b#6[$NtDZ84Je2KNvB#$P5?tQ3nt(0"
    "d=j.LQf./Ll33+(;q3L-w=8dX$#WF&uIJ@-bfI>%:_i2B5CsR8&9Z&#=mPEnm0f`<&c)QL5uJ#%u%lJj+D-r;BoF&#4DoS97h5g)E#o:&S4weDF,9^Hoe`h*L+_a*NrLW-1pG_&2UdB8"
    "6e%B/:=>)N4xeW.*wft-;$'58-ESqr<b?UI(_%@[P46>#U`'6AQ]m&6/`Z>#S?YY#Vc;r7U2&326d=w&H####?TZ`*4?&.MK?LP8Vxg>$[QXc%QJv92.(Db*B)gb*BM9dM*hJMAo*c&#"
    "b0v=Pjer]$gG&JXDf->'StvU7505l9$AFvgYRI^&<^b68?j#q9QX4SM'RO#&sL1IM.rJfLUAj221]d##DW=m83u5;'bYx,*Sl0hL(W;;$doB&O/TQ:(Z^xBdLjL<Lni;''X.`$#8+1GD"
    ":k$YUWsbn8ogh6rxZ2Z9]%nd+>V#*8U_72Lh+2Q8Cj0i:6hp&$C/:p(HK>T8Y[gHQ4`4)'$Ab(Nof%V'8hL&#<NEdtg(n'=S1A(Q1/I&4([%dM`,Iu'1:_hL>SfD07&6D<fp8dHM7/g+"
    "tlPN9J*rKaPct&?'uBCem^jn%9_K)<,C5K3s=5g&GmJb*[SYq7K;TRLGCsM-$$;S%:Y@r7AK0pprpL<Lrh,q7e/%KWK:50I^+m'vi`3?%Zp+<-d+$L-Sv:@.o19n$s0&39;kn;S%BSq*"
    "$3WoJSCLweV[aZ'MQIjO<7;X-X;&+dMLvu#^UsGEC9WEc[X(wI7#2.(F0jV*eZf<-Qv3J-c+J5AlrB#$p(H68LvEA'q3n0#m,[`*8Ft)FcYgEud]CWfm68,(aLA$@EFTgLXoBq/UPlp7"
    ":d[/;r_ix=:TF`S5H-b<LI&HY(K=h#)]Lk$K14lVfm:x$H<3^Ql<M`$OhapBnkup'D#L$Pb_`N*g]2e;X/Dtg,bsj&K#2[-:iYr'_wgH)NUIR8a1n#S?Yej'h8^58UbZd+^FKD*T@;6A"
    "7aQC[K8d-(v6GI$x:T<&'Gp5Uf>@M.*J:;$-rv29'M]8qMv-tLp,'886iaC=Hb*YJoKJ,(j%K=H`K.v9HggqBIiZu'QvBT.#=)0ukruV&.)3=(^1`o*Pj4<-<aN((^7('#Z0wK#5GX@7"
    "u][`*S^43933A4rl][`*O4CgLEl]v$1Q3AeF37dbXk,.)vj#x'd`;qgbQR%FW,2(?LO=s%Sc68%NP'##Aotl8x=BE#j1UD([3$M(]UI2LX3RpKN@;/#f'f/&_mt&F)XdF<9t4)Qa.*kT"
    "LwQ'(TTB9.xH'>#MJ+gLq9-##@HuZPN0]u:h7.T..G:;$/Usj(T7`Q8tT72LnYl<-qx8;-HV7Q-&Xdx%1a,hC=0u+HlsV>nuIQL-5<N?)NBS)QN*_I,?&)2'IM%L3I)X((e/dl2&8'<M"
    ":^#M*Q+[T.Xri.LYS3v%fF`68h;b-X[/En'CR.q7E)p'/kle2HM,u;^%OKC-N+Ll%F9CF<Nf'^#t2L,;27W:0O@6##U6W7:$rJfLWHj$#)woqBefIZ.PK<b*t7ed;p*_m;4ExK#h@&]>"
    "_>@kXQtMacfD.m-VAb8;IReM3$wf0''hra*so568'Ip&vRs849'MRYSp%:t:h5qSgwpEr$B>Q,;s(C#$)`svQuF$##-D,##,g68@2[T;.XSdN9Qe)rpt._K-#5wF)sP'##p#C0c%-Gb%"
    "hd+<-j'Ai*x&&HMkT]C'OSl##5RG[JXaHN;d'uA#x._U;.`PU@(Z3dt4r152@:v,'R.Sj'w#0<-;kPI)FfJ&#AYJ&#//)>-k=m=*XnK$>=)72L]0I%>.G690a:$##<,);?;72#?x9+d;"
    "^V'9;jY@;)br#q^YQpx:X#Te$Z^'=-=bGhLf:D6&bNwZ9-ZD#n^9HhLMr5G;']d&6'wYmTFmL<LD)F^%[tC'8;+9E#C$g%#5Y>q9wI>P(9mI[>kC-ekLC/R&CH+s'B;K-M6$EB%is00:"
    "+A4[7xks.LrNk0&E)wILYF@2L'0Nb$+pv<(2.768/FrY&h$^3i&@+G%JT'<-,v`3;_)I9M^AE]CN?Cl2AZg+%4iTpT3<n-&%H%b<FDj2M<hH=&Eh<2Len$b*aTX=-8QxN)k11IM1c^j%"
    "9s<L<NFSo)B?+<-(GxsF,^-Eh@$4dXhN$+#rxK8'je'D7k`e;)2pYwPA'_p9&@^18ml1^[@g4t*[JOa*[=Qp7(qJ_oOL^('7fB&Hq-:sf,sNj8xq^>$U4O]GKx'm9)b@p7YsvK3w^YR-"
    "CdQ*:Ir<($u&)#(&?L9Rg3H)4fiEp^iI9O8KnTj,]H?D*r7'M;PwZ9K0E^k&-cpI;.p/6_vwoFMV<->#%Xi.LxVnrU(4&8/P+:hLSKj$#U%]49t'I:rgMi'FL@a:0Y-uA[39',(vbma*"
    "hU%<-SRF`Tt:542R_VV$p@[p8DV[A,?1839FWdF<TddF<9Ah-6&9tWoDlh]&1SpGMq>Ti1O*H&#(AL8[_P%.M>v^-))qOT*F5Cq0`Ye%+$B6i:7@0IX<N+T+0MlMBPQ*Vj>SsD<U4JHY"
    "8kD2)2fU/M#$e.)T4,_=8hLim[&);?UkK'-x?'(:siIfL<$pFM`i<?%W(mGDHM%>iWP,##P`%/L<eXi:@Z9C.7o=@(pXdAO/NLQ8lPl+HPOQa8wD8=^GlPa8TKI1CjhsCTSLJM'/Wl>-"
    "S(qw%sf/@%#B6;/U7K]uZbi^Oc^2n<bhPmUkMw>%t<)'mEVE''n`WnJra$^TKvX5B>;_aSEK',(hwa0:i4G?.Bci.(X[?b*($,=-n<.Q%`(X=?+@Am*Js0&=3bh8K]mL<LoNs'6,'85`"
    "0?t/'_U59@]ddF<#LdF<eWdF<OuN/45rY<-L@&#+fm>69=Lb,OcZV/);TTm8VI;?%OtJ<(b4mq7M6:u?KRdF<gR@2L=FNU-<b[(9c/ML3m;Z[$oF3g)GAWqpARc=<ROu7cL5l;-[A]%/"
    "+fsd;l#SafT/f*W]0=O'$(Tb<[)*@e775R-:Yob%g*>l*:xP?Yb.5)%w_I?7uk5JC+FS(m#i'k.'a0i)9<7b'fs'59hq$*5Uhv##pi^8+hIEBF`nvo`;'l0.^S1<-wUK2/Coh58KKhLj"
    "M=SO*rfO`+qC`W-On.=AJ56>>i2@2LH6A:&5q`?9I3@@'04&p2/LVa*T-4<-i3;M9UvZd+N7>b*eIwg:CC)c<>nO&#<IGe;__.thjZl<%w(Wk2xmp4Q@I#I9,DF]u7-P=.-_:YJ]aS@V"
    "?6*C()dOp7:WL,b&3Rg/.cmM9&r^>$(>.Z-I&J(Q0Hd5Q%7Co-b`-c<N(6r@ip+AurK<m86QIth*#v;-OBqi+L7wDE-Ir8K['m+DDSLwK&/.?-V%U_%3:qKNu$_b*B-kp7NaD'QdWQPK"
    "Yq[@>P)hI;*_F]u`Rb[.j8_Q/<&>uu+VsH$sM9TA%?)(vmJ80),P7E>)tjD%2L=-t#fK[%`v=Q8<FfNkgg^oIbah*#8/Qt$F&:K*-(N/'+1vMB,u()-a.VUU*#[e%gAAO(S>WlA2);Sa"
    ">gXm8YB`1d@K#n]76-a$U,mF<fX]idqd)<3,]J7JmW4`6]uks=4-72L(jEk+:bJ0M^q-8Dm_Z?0olP1C9Sa&H[d&c$ooQUj]Exd*3ZM@-WGW2%s',B-_M%>%Ul:#/'xoFM9QX-$.QN'>"
    "[%$Z$uF6pA6Ki2O5:8w*vP1<-1`[G,)-m#>0`P&#eb#.3i)rtB61(o'$?X3B</R90;eZ]%Ncq;-Tl]#F>2Qft^ae_5tKL9MUe9b*sLEQ95C&`=G?@Mj=wh*'3E>=-<)Gt*Iw)'QG:`@I"
    "wOf7&]1i'S01B+Ev/Nac#9S;=;YQpg_6U`*kVY39xK,[/6Aj7:'1Bm-_1EYfa1+o&o4hp7KN_Q(OlIo@S%;jVdn0'1<Vc52=u`3^o-n1'g4v58Hj&6_t7$##?M)c<$bgQ_'SY((-xkA#"
    "Y(,p'H9rIVY-b,'%bCPF7.J<Up^,(dU1VY*5#WkTU>h19w,WQhLI)3S#f$2(eb,jr*b;3Vw]*7NH%$c4Vs,eD9>XW8?N]o+(*pgC%/72LV-u<Hp,3@e^9UB1J+ak9-TN/mhKPg+AJYd$"
    "MlvAF_jCK*.O-^(63adMT->W%iewS8W6m2rtCpo'RS1R84=@paTKt)>=%&1[)*vp'u+x,VrwN;&]kuO9JDbg=pO$J*.jVe;u'm0dr9l,<*wMK*Oe=g8lV_KEBFkO'oU]^=[-792#ok,)"
    "i]lR8qQ2oA8wcRCZ^7w/Njh;?.stX?Q1>S1q4Bn$)K1<-rGdO'$Wr.Lc.CG)$/*JL4tNR/,SVO3,aUw'DJN:)Ss;wGn9A32ijw%FL+Z0Fn.U9;reSq)bmI32U==5ALuG&#Vf1398/pVo"
    "1*c-(aY168o<`JsSbk-,1N;$>0:OUas(3:8Z972LSfF8eb=c-;>SPw7.6hn3m`9^Xkn(r.qS[0;T%&Qc=+STRxX'q1BNk3&*eu2;&8q$&x>Q#Q7^Tf+6<(d%ZVmj2bDi%.3L2n+4W'$P"
    "iDDG)g,r%+?,$@?uou5tSe2aN_AQU*<h`e-GI7)?OK2A.d7_c)?wQ5AS@DL3r#7fSkgl6-++D:'A,uq7SvlB$pcpH'q3n0#_%dY#xCpr-l<F0NR@-##FEV6NTF6##$l84N1w?AO>'IAO"
    "URQ##V^Fv-XFbGM7Fl(N<3DhLGF%q.1rC$#:T__&Pi68%0xi_&[qFJ(77j_&JWoF.V735&T,[R*:xFR*K5>>#`bW-?4Ne_&6Ne_&6Ne_&n`kr-#GJcM6X;uM6X;uM(.a..^2TkL%oR(#"
    ";u.T%fAr%4tJ8&><1=GHZ_+m9/#H1F^R#SC#*N=BA9(D?v[UiFY>>^8p,KKF.W]L29uLkLlu/+4T<XoIB&hx=T1PcDaB&;HH+-AFr?(m9HZV)FKS8JCw;SD=6[^/DZUL`EUDf]GGlG&>"
    "w$)F./^n3+rlo+DB;5sIYGNk+i1t-69Jg--0pao7Sm#K)pdHW&;LuDNH@H>#/X-TI(;P>#,Gc>#0Su>#4`1?#8lC?#<xU?#@.i?#D:%@#HF7@#LRI@#P_[@#Tkn@#Xw*A#]-=A#a9OA#"
    "d<F&#*;G##.GY##2Sl##6`($#:l:$#>xL$#B.`$#F:r$#JF.%#NR@%#R_R%#Vke%#Zww%#_-4&#3^Rh%Sflr-k'MS.o?.5/sWel/wpEM0%3'/1)K^f1-d>G21&v(35>V`39V7A4=onx4"
    "A1OY5EI0;6Ibgr6M$HS7Q<)58C5w,;WoA*#[%T*#`1g*#d=#+#hI5+#lUG+#pbY+#tnl+#x$),#&1;,#*=M,#.I`,#2Ur,#6b.-#;w[H#iQtA#m^0B#qjBB#uvTB##-hB#'9$C#+E6C#"
    "/QHC#3^ZC#7jmC#;v)D#?,<D#C8ND#GDaD#KPsD#O]/E#g1A5#KA*1#gC17#MGd;#8(02#L-d3#rWM4#Hga1#,<w0#T.j<#O#'2#CYN1#qa^:#_4m3#o@/=#eG8=#t8J5#`+78#4uI-#"
    "m3B2#SB[8#Q0@8#i[*9#iOn8#1Nm;#^sN9#qh<9#:=x-#P;K2#$%X9#bC+.#Rg;<#mN=.#MTF.#RZO.#2?)4#Y#(/#[)1/#b;L/#dAU/#0Sv;#lY$0#n`-0#sf60#(F24#wrH0#%/e0#"
    "TmD<#%JSMFove:CTBEXI:<eh2g)B,3h2^G3i;#d3jD>)4kMYD4lVu`4m`:&5niUA5@(A5BA1]PBB:xlBCC=2CDLXMCEUtiCf&0g2'tN?PGT4CPGT4CPGT4CPGT4CPGT4CPGT4CPGT4CP"
    "GT4CPGT4CPGT4CPGT4CPGT4CPGT4CP-qekC`.9kEg^+F$kwViFJTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5o,^<-28ZI'O?;xp"
    "O?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xp;7q-#lLYI:xvD=#";

static void
pl__add_default_font(plFontAtlas* ptrAtlas)
{
    static const char* cPtrEmbeddedFontName = "Proggy.ttf";
    
    void* data = NULL;

    int iCompressedTTFSize = (((int)strlen(gcPtrDefaultFontCompressed) + 4) / 5) * 4;
    void* ptrCompressedTTF = PL_ALLOC((size_t)iCompressedTTFSize);
    pl__decode85((const unsigned char*)gcPtrDefaultFontCompressed, (unsigned char*)ptrCompressedTTF);

    const uint32_t uDecompressedSize = pl__decompress_length((const unsigned char*)ptrCompressedTTF);
    data = (unsigned char*)PL_ALLOC(uDecompressedSize);
    pl__decompress((unsigned char*)data, (const unsigned char*)ptrCompressedTTF, (int)iCompressedTTFSize);

    PL_FREE(ptrCompressedTTF);

    plFontConfig fontConfig = {
        .sdf = false,
        .fontSize = 13.0f,
        .hOverSampling = 1,
        .vOverSampling = 1,
        .onEdgeValue = 255,
        .sdfPadding = 1
    };
    
    plFontRange range = {
        .firstCodePoint = 0x0020,
        .charCount = 0x00FF - 0x0020
    };
    pl_sb_push(fontConfig.sbRanges, range);
    pl__add_font_from_memory_ttf(ptrAtlas, fontConfig, data);
}

const plDrawApiI*
pl_load_draw_api(void)
{
    static const plDrawApiI tApi0 = {

        // provided by backend
        .new_frame                = pl_new_draw_frame,
        .cleanup_context          = pl_cleanup_draw_context,
        .build_font_atlas         = pl_build_font_atlas,
        .cleanup_font_atlas       = pl_cleanup_font_atlas,

        // provided by this file
        .set_context              = pl__set_draw_context,
        .get_context              = pl__get_draw_context,
        .register_drawlist        = pl__register_drawlist,
        .register_3d_drawlist     = pl__register_3d_drawlist,
        .request_layer            = pl__request_draw_layer,
        .return_layer             = pl__return_draw_layer,
        .submit_layer             = pl__submit_draw_layer,
        .add_line                 = pl__add_line,
        .add_lines                = pl__add_lines,
        .add_text                 = pl__add_text,
        .add_text_ex              = pl__add_text_ex,
        .add_text_clipped         = pl__add_text_clipped,
        .add_text_clipped_ex      = pl__add_text_clipped_ex,
        .add_triangle             = pl__add_triangle,
        .add_triangle_filled      = pl__add_triangle_filled,
        .add_rect                 = pl__add_rect,
        .add_rect_filled          = pl__add_rect_filled,
        .add_rect_rounded         = pl__add_rect_rounded,
        .add_rect_rounded_filled  = pl__add_rect_rounded_filled,
        .add_quad                 = pl__add_quad,
        .add_quad_filled          = pl__add_quad_filled,
        .add_circle               = pl__add_circle,
        .add_circle_filled        = pl__add_circle_filled,
        .add_image                = pl__add_image,
        .add_image_ex             = pl__add_image_ex,
        .add_bezier_quad          = pl__add_bezier_quad,
        .add_bezier_cubic         = pl__add_bezier_cubic,
        .add_3d_triangle_filled   = pl__add_3d_triangle_filled,
        .add_3d_line              = pl__add_3d_line,
        .add_3d_point             = pl__add_3d_point,
        .add_3d_transform         = pl__add_3d_transform,
        .add_3d_frustum           = pl__add_3d_frustum,
        .add_3d_centered_box      = pl__add_3d_centered_box,
        .add_3d_bezier_quad       = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic      = pl__add_3d_bezier_cubic,
        .add_default_font         = pl__add_default_font,
        .add_font_from_file_ttf   = pl__add_font_from_file_ttf,
        .add_font_from_memory_ttf = pl__add_font_from_memory_ttf,
        .calculate_text_size      = pl__calculate_text_size,
        .calculate_text_size_ex   = pl__calculate_text_size_ex,
        .calculate_text_bb        = pl__calculate_text_bb,
        .calculate_text_bb_ex     = pl__calculate_text_bb_ex,
        .push_clip_rect_pt        = pl__push_clip_rect_pt,
        .push_clip_rect           = pl__push_clip_rect,
        .pop_clip_rect            = pl__pop_clip_rect,
        .get_clip_rect            = pl__get_clip_rect
    };
    return &tApi0;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_draw_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDrawApiI* ptDrawApi = pl_load_draw_api();
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    if(bReload)
    { 
        ptDrawApi->set_context(ptDataRegistry->get_data("pilotlight draw"));
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DRAW), ptDrawApi);
    }
    else
    {
        ptApiRegistry->add(PL_API_DRAW, ptDrawApi);
        ptDataRegistry->set_data("pilotlight draw", pl__get_draw_context());
    }

    #ifdef PL_METAL_BACKEND
    if(bReload)
    {
        ptDrawApi->set_context(ptDataRegistry->get_data("pilotlight draw"));
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_METAL_DRAW), pl_load_metal_draw_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_METAL_DRAW, pl_load_metal_draw_api());
        ptDataRegistry->set_data("pilotlight draw", pl__get_draw_context());
    }
    #endif

    #ifdef PL_VULKAN_BACKEND
    if(bReload)
    {
        ptDrawApi->set_context(ptDataRegistry->get_data("pilotlight draw"));
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_VULKAN_DRAW), pl_load_vulkan_draw_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_VULKAN_DRAW, pl_load_vulkan_draw_api());
        ptDataRegistry->set_data("pilotlight draw", pl__get_draw_context());
    }
    #endif
}

PL_EXPORT void
pl_unload_draw_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, U) PL_ALLOC(x)
#define STBTT_free(x, U) PL_FREE(x)
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION

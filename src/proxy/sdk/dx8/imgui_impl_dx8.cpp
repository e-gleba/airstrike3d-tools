#include "imgui_impl_dx8.h"

#include "imgui.h"

#include <d3d8.h>
#include <cstdio>
#include <cstring>

// ─── Vertex format ───────────────────────────────────────────────────────────

struct ImVert final
{
    float    x, y, z, rhw;
    D3DCOLOR col;
    float    u, v;
};

inline constexpr auto k_fvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

// ─── Backend state ───────────────────────────────────────────────────────────

static IDirect3DDevice8*       g_dev   = nullptr;
static IDirect3DVertexBuffer8* g_vb    = nullptr;
static IDirect3DIndexBuffer8*  g_ib    = nullptr;
static int                     g_vb_sz = 0;
static int                     g_ib_sz = 0;

// Captured render state for restore
struct saved_state final
{
    DWORD fillmode, shade_mode;
    DWORD zwrite, zfunc, alpha_blend, alpha_src, alpha_dst;
    DWORD fog, stencil;
    DWORD cull, zenable;
    DWORD color_op, color_arg1, color_arg2;
    DWORD alpha_op, alpha_arg1, alpha_arg2;
    DWORD tfactor;
    DWORD mag_filter, min_filter, mip_filter;
    DWORD lighting, ambient;
    DWORD fvf;
    DWORD address_u, address_v;
};

static void capture_state(saved_state& ss)
{
    g_dev->GetRenderState(D3DRS_FILLMODE,            &ss.fillmode);
    g_dev->GetRenderState(D3DRS_SHADEMODE,           &ss.shade_mode);
    g_dev->GetRenderState(D3DRS_ZWRITEENABLE,        &ss.zwrite);
    g_dev->GetRenderState(D3DRS_ZFUNC,               &ss.zfunc);
    g_dev->GetRenderState(D3DRS_ALPHABLENDENABLE,    &ss.alpha_blend);
    g_dev->GetRenderState(D3DRS_SRCBLEND,            &ss.alpha_src);
    g_dev->GetRenderState(D3DRS_DESTBLEND,           &ss.alpha_dst);
    g_dev->GetRenderState(D3DRS_FOGENABLE,           &ss.fog);
    g_dev->GetRenderState(D3DRS_STENCILENABLE,       &ss.stencil);
    g_dev->GetRenderState(D3DRS_CULLMODE,            &ss.cull);
    g_dev->GetRenderState(D3DRS_ZENABLE,             &ss.zenable);
    g_dev->GetRenderState(D3DRS_TEXTUREFACTOR,       &ss.tfactor);
    g_dev->GetRenderState(D3DRS_LIGHTING,            &ss.lighting);
    g_dev->GetRenderState(D3DRS_AMBIENT,             &ss.ambient);

    g_dev->GetTextureStageState(0, D3DTSS_COLOROP,   &ss.color_op);
    g_dev->GetTextureStageState(0, D3DTSS_COLORARG1, &ss.color_arg1);
    g_dev->GetTextureStageState(0, D3DTSS_COLORARG2, &ss.color_arg2);
    g_dev->GetTextureStageState(0, D3DTSS_ALPHAOP,   &ss.alpha_op);
    g_dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &ss.alpha_arg1);
    g_dev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &ss.alpha_arg2);

    g_dev->GetTextureStageState(0, D3DTSS_MAGFILTER, &ss.mag_filter);
    g_dev->GetTextureStageState(0, D3DTSS_MINFILTER, &ss.min_filter);
    g_dev->GetTextureStageState(0, D3DTSS_MIPFILTER, &ss.mip_filter);

    g_dev->GetTextureStageState(0, D3DTSS_ADDRESSU,  &ss.address_u);
    g_dev->GetTextureStageState(0, D3DTSS_ADDRESSV,  &ss.address_v);

    // D3D8 uses GetVertexShader/SetVertexShader for FVF codes
    g_dev->GetVertexShader(&ss.fvf);
}

static void restore_state(const saved_state& ss)
{
    g_dev->SetRenderState(D3DRS_FILLMODE,            ss.fillmode);
    g_dev->SetRenderState(D3DRS_SHADEMODE,           ss.shade_mode);
    g_dev->SetRenderState(D3DRS_ZWRITEENABLE,        ss.zwrite);
    g_dev->SetRenderState(D3DRS_ZFUNC,               ss.zfunc);
    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE,    ss.alpha_blend);
    g_dev->SetRenderState(D3DRS_SRCBLEND,            ss.alpha_src);
    g_dev->SetRenderState(D3DRS_DESTBLEND,           ss.alpha_dst);
    g_dev->SetRenderState(D3DRS_FOGENABLE,           ss.fog);
    g_dev->SetRenderState(D3DRS_STENCILENABLE,       ss.stencil);
    g_dev->SetRenderState(D3DRS_CULLMODE,            ss.cull);
    g_dev->SetRenderState(D3DRS_ZENABLE,             ss.zenable);
    g_dev->SetRenderState(D3DRS_TEXTUREFACTOR,       ss.tfactor);
    g_dev->SetRenderState(D3DRS_LIGHTING,            ss.lighting);
    g_dev->SetRenderState(D3DRS_AMBIENT,             ss.ambient);

    g_dev->SetTextureStageState(0, D3DTSS_COLOROP,   ss.color_op);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG1, ss.color_arg1);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG2, ss.color_arg2);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   ss.alpha_op);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, ss.alpha_arg1);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, ss.alpha_arg2);

    g_dev->SetTextureStageState(0, D3DTSS_MAGFILTER, ss.mag_filter);
    g_dev->SetTextureStageState(0, D3DTSS_MINFILTER, ss.min_filter);
    g_dev->SetTextureStageState(0, D3DTSS_MIPFILTER, ss.mip_filter);

    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSU,  ss.address_u);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSV,  ss.address_v);

    g_dev->SetVertexShader(ss.fvf);
}

static void set_render_state()
{
    g_dev->SetRenderState(D3DRS_FILLMODE,         D3DFILL_SOLID);
    g_dev->SetRenderState(D3DRS_SHADEMODE,        D3DSHADE_GOURAUD);
    g_dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
    g_dev->SetRenderState(D3DRS_ZFUNC,            D3DCMP_ALWAYS);
    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_dev->SetRenderState(D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
    g_dev->SetRenderState(D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
    g_dev->SetRenderState(D3DRS_FOGENABLE,        FALSE);
    g_dev->SetRenderState(D3DRS_STENCILENABLE,    FALSE);
    g_dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
    g_dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
    g_dev->SetRenderState(D3DRS_LIGHTING,         FALSE);

    g_dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    g_dev->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    g_dev->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    g_dev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP);
    g_dev->SetTextureStageState(0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP);

    g_dev->SetVertexShader(k_fvf);
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device)
{
    g_dev = device;
    g_dev->AddRef();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_dx8";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    return true;
}

void ImGui_ImplDX8_Shutdown()
{
    ImGui_ImplDX8_InvalidateDeviceObjects();

    if (g_dev != nullptr)
    {
        g_dev->Release();
        g_dev = nullptr;
    }
}

void ImGui_ImplDX8_NewFrame()
{
    // Nothing to do — buffers are created on demand in RenderDrawData
}

static void setup_render_state(ImDrawData* draw_data)
{
    D3DVIEWPORT8 vp{};
    vp.X      = 0;
    vp.Y      = 0;
    vp.Width  = static_cast<DWORD>(draw_data->DisplaySize.x);
    vp.Height = static_cast<DWORD>(draw_data->DisplaySize.y);
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    g_dev->SetViewport(&vp);

    // Orthographic projection matrix
    float L = 0.0f;
    float R = draw_data->DisplaySize.x;
    float T = 0.0f;
    float B = draw_data->DisplaySize.y;

    float mat[16] = {
        2.0f/(R-L),     0.0f,            0.0f,  0.0f,
        0.0f,           2.0f/(T-B),      0.0f,  0.0f,
        0.0f,           0.0f,            0.5f,  0.0f,
        (L+R)/(L-R),    (T+B)/(B-T),     0.5f,  1.0f,
    };

    auto* d3d_mat = reinterpret_cast<const D3DMATRIX*>(&mat);
    g_dev->SetTransform(D3DTS_PROJECTION, d3d_mat);
    g_dev->SetTransform(D3DTS_VIEW,       d3d_mat);
    g_dev->SetTransform(D3DTS_WORLD,      d3d_mat);
}

void ImGui_ImplDX8_RenderDrawData(ImDrawData* draw_data)
{
    if ((g_dev == nullptr) || (draw_data->DisplaySize.x <= 0.0f)
        || (draw_data->DisplaySize.y <= 0.0f))
    {
        return;
    }

    // Create/grow vertex buffer
    const int vtx_needed = draw_data->TotalVtxCount;
    if ((g_vb == nullptr) || (g_vb_sz < vtx_needed))
    {
        if (g_vb != nullptr) { g_vb->Release(); }

        int new_sz = vtx_needed + 5000;
        if (FAILED(g_dev->CreateVertexBuffer(
                static_cast<UINT>(new_sz) * sizeof(ImVert),
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                k_fvf, D3DPOOL_DEFAULT, &g_vb)))
        {
            return;
        }
        g_vb_sz = new_sz;
    }

    // Create/grow index buffer
    const int idx_needed = draw_data->TotalIdxCount;
    if ((g_ib == nullptr) || (g_ib_sz < idx_needed))
    {
        if (g_ib != nullptr) { g_ib->Release(); }

        int new_sz = idx_needed + 10000;
        if (FAILED(g_dev->CreateIndexBuffer(
                static_cast<UINT>(new_sz) * sizeof(ImDrawIdx),
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                D3DFMT_INDEX16, D3DPOOL_DEFAULT, &g_ib)))
        {
            return;
        }
        g_ib_sz = new_sz;
    }

    // Copy vertices and indices
    {
        ImVert* vtx_dst = nullptr;
        ImDrawIdx* idx_dst = nullptr;

        g_vb->Lock(0, static_cast<UINT>(vtx_needed) * sizeof(ImVert),
                   reinterpret_cast<BYTE**>(&vtx_dst), D3DLOCK_DISCARD);
        g_ib->Lock(0, static_cast<UINT>(idx_needed) * sizeof(ImDrawIdx),
                   reinterpret_cast<BYTE**>(&idx_dst), D3DLOCK_DISCARD);

        for (int n = 0; n < draw_data->CmdListsCount; ++n)
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];

            for (int i = 0; i < cmd_list->VtxBuffer.Size; ++i)
            {
                const auto& iv = cmd_list->VtxBuffer[i];
                vtx_dst->x   = iv.pos.x;
                vtx_dst->y   = iv.pos.y;
                vtx_dst->z   = 0.0f;
                vtx_dst->rhw = 1.0f;
                vtx_dst->col = (iv.col & 0xFF00FF00)
                             | ((iv.col & 0xFF0000) >> 16)
                             | ((iv.col & 0xFF) << 16);
                vtx_dst->u   = iv.uv.x;
                vtx_dst->v   = iv.uv.y;
                ++vtx_dst;
            }

            const auto idx_bytes = static_cast<size_t>(cmd_list->IdxBuffer.Size)
                                 * sizeof(ImDrawIdx);
            std::memcpy(idx_dst, cmd_list->IdxBuffer.Data, idx_bytes);
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        g_vb->Unlock();
        g_ib->Unlock();
    }

    // Save state, set our state, render, restore
    saved_state ss;
    capture_state(ss);
    set_render_state();
    setup_render_state(draw_data);

    g_dev->SetStreamSource(0, g_vb, sizeof(ImVert));
    g_dev->SetIndices(g_ib, 0);

    // Iterate command lists
    int    global_vtx_ofs = 0;
    int    global_idx_ofs = 0;
    D3DVIEWPORT8 saved_vp{};
    g_dev->GetViewport(&saved_vp);

    for (int n = 0; n < draw_data->CmdListsCount; ++n)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        for (int i = 0; i < cmd_list->CmdBuffer.Size; ++i)
        {
            const ImDrawCmd* cmd = &cmd_list->CmdBuffer[i];

            if (cmd->UserCallback != nullptr)
            {
                cmd->UserCallback(cmd_list, cmd);
            }
            else
            {
                // D3D8 doesn't support scissor test natively.
                // Clip rects are ignored — imgui still renders correctly
                // because vertices are already clipped.

                // Set texture (GetTexID returns ImTextureID = void* / uint64)
                g_dev->SetTexture(0, reinterpret_cast<IDirect3DBaseTexture8*>(
                                         cmd->GetTexID()));

                g_dev->DrawIndexedPrimitive(
                    D3DPT_TRIANGLELIST,
                    static_cast<UINT>(global_vtx_ofs),
                    static_cast<UINT>(cmd_list->VtxBuffer.Size),
                    static_cast<UINT>(cmd->IdxOffset)
                        + static_cast<UINT>(global_idx_ofs),
                    cmd->ElemCount / 3);
            }
        }
        global_vtx_ofs += cmd_list->VtxBuffer.Size;
        global_idx_ofs += cmd_list->IdxBuffer.Size;
    }

    g_dev->SetViewport(&saved_vp);
    restore_state(ss);
}

void ImGui_ImplDX8_InvalidateDeviceObjects()
{
    if (g_vb != nullptr) { g_vb->Release(); g_vb = nullptr; }
    if (g_ib != nullptr) { g_ib->Release(); g_ib = nullptr; }
    g_vb_sz = 0;
    g_ib_sz = 0;
}

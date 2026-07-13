/*
 * amd_ags_x64.dll proxy shim for Anno 117 under CrossOver / D3DMetal.
 *
 * The game reaches its swapchain through NVIDIA Streamline (sl.interposer),
 * which calls the real D3DMetal DXGI factory's CreateSwapChainForComposition
 * -- unimplemented by D3DMetal (E_NOTIMPL / 0x80004001), so the game crashes.
 *
 * amd_ags_x64.dll is a static import of the game and a normal native PE, so
 * we proxy it: its three imported functions are forwarded (via the .def) to a
 * renamed copy amd_ags_orig.dll, and DllMain spawns a worker that patches the
 * shared D3DMetal IDXGIFactory vtable so that slot 24
 * (CreateSwapChainForComposition) instead invokes slot 15
 * (CreateSwapChainForHwnd) bound to the game's own top-level window -- a path
 * D3DMetal fully supports. The vtable is process-wide, so every factory the
 * game or Streamline later creates inherits the redirect.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { UINT Count; UINT Quality; } DXGI_SAMPLE_DESC;
typedef struct {
    UINT Width, Height, Format; BOOL Stereo;
    DXGI_SAMPLE_DESC SampleDesc; UINT BufferUsage, BufferCount;
    int Scaling, SwapEffect, AlphaMode; UINT Flags;
} DXGI_SWAP_CHAIN_DESC1;

#define DXGI_ALPHA_MODE_UNSPECIFIED 0
#define DXGI_SCALING_STRETCH 0
#define DXGI_SCALING_ASPECT_RATIO_STRETCH 2
#define DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER 0x20
#define IDX_CreateSwapChainForHwnd 15
#define IDX_CreateSwapChainForComposition 24

typedef HRESULT (WINAPI *PFN_CreateForHwnd)(void *This, IUnknown *pDevice, HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1 *pDesc, const void *pFullscreenDesc,
        void *pRestrictToOutput, void **ppSwapChain);
typedef HRESULT (WINAPI *PFN_Factory2)(UINT flags, REFIID riid, void **pp);
typedef HRESULT (WINAPI *PFN_Factory)(REFIID riid, void **pp);
typedef ULONG   (WINAPI *PFN_Release)(void *This);

/* IID_IDXGIFactory2 = {50c83a1c-e072-4c48-87b0-3630fa36a6d0} */
static const GUID IID_IDXGIFactory2 =
    {0x50c83a1c,0xe072,0x4c48,{0x87,0xb0,0x36,0x30,0xfa,0x36,0xa6,0xd0}};

static HINSTANCE g_self = NULL;
static volatile LONG g_patched = 0;

static void shim_log(const char *fmt, ...) {
    FILE *f = fopen("C:\\windows\\temp\\ags_shim.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

typedef struct { HWND best; LONG bestArea; } FindCtx;
static BOOL CALLBACK enum_cb(HWND hwnd, LPARAM lp) {
    FindCtx *c = (FindCtx*)lp;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    RECT r; if (!GetWindowRect(hwnd, &r)) return TRUE;
    LONG area = (r.right - r.left) * (LONG)(r.bottom - r.top);
    if (area > c->bestArea) { c->bestArea = area; c->best = hwnd; }
    return TRUE;
}
static HWND find_game_window(void) {
    FindCtx c = { NULL, 0 };
    EnumWindows(enum_cb, (LPARAM)&c);
    if (!c.best) c.best = GetForegroundWindow();
    return c.best;
}

static HRESULT WINAPI My_CreateSwapChainForComposition(void *This, IUnknown *pDevice,
        const DXGI_SWAP_CHAIN_DESC1 *pDesc, void *pRestrictToOutput, void **ppSwapChain) {
    void **vt = *(void***)This;
    PFN_CreateForHwnd real_hwnd = (PFN_CreateForHwnd)vt[IDX_CreateSwapChainForHwnd];
    HWND hwnd = find_game_window();
    DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    if (desc.Scaling == DXGI_SCALING_ASPECT_RATIO_STRETCH) desc.Scaling = DXGI_SCALING_STRETCH;
    desc.Flags &= ~DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER;
    shim_log("[shim] Composition->Hwnd hwnd=%p %ux%u fmt=%u count=%u scaling=%d effect=%d flags=0x%x",
             (void*)hwnd, desc.Width, desc.Height, desc.Format, desc.BufferCount,
             desc.Scaling, desc.SwapEffect, desc.Flags);
    HRESULT hr = real_hwnd(This, pDevice, hwnd, &desc, NULL, pRestrictToOutput, ppSwapChain);
    shim_log("[shim] CreateSwapChainForHwnd -> hr=0x%08x swapchain=%p",
             (unsigned)hr, ppSwapChain ? *ppSwapChain : NULL);
    return hr;
}

/* Offset of the D3DMetal IDXGIFactory vtable within dxgi.dll (discovered at
 * runtime via GetModuleHandleExA; stable because dxgi.dll's .rdata layout is
 * fixed for this build). We patch it by module+offset rather than creating a
 * factory ourselves -- creating an out-of-band DXGI factory poisons the game's
 * adapter enumeration and makes it fail with error -1213. */
#define DXGI_FACTORY_VTABLE_OFFSET 0x1a9c0

static BOOL patch_vtable(void **vt) {
    if (vt[IDX_CreateSwapChainForComposition] == (void*)My_CreateSwapChainForComposition)
        return TRUE; /* already patched */
    if (!vt[IDX_CreateSwapChainForComposition] || !vt[IDX_CreateSwapChainForHwnd]) {
        shim_log("[shim] vtable %p looks wrong (slot15=%p slot24=%p) - not patching",
                 (void*)vt, vt[IDX_CreateSwapChainForHwnd],
                 vt[IDX_CreateSwapChainForComposition]);
        return FALSE;
    }
    DWORD old;
    if (VirtualProtect(&vt[IDX_CreateSwapChainForComposition], sizeof(void*),
                       PAGE_EXECUTE_READWRITE, &old)) {
        vt[IDX_CreateSwapChainForComposition] = (void*)My_CreateSwapChainForComposition;
        VirtualProtect(&vt[IDX_CreateSwapChainForComposition], sizeof(void*), old, &old);
        shim_log("[shim] PATCHED dxgi vtable=%p slot24", (void*)vt);
        return TRUE;
    }
    shim_log("[shim] VirtualProtect FAILED vtable=%p", (void*)vt);
    return FALSE;
}

static DWORD WINAPI worker(LPVOID param) {
    (void)param;
    /* Create a DXGI factory ourselves so D3DMetal populates/returns its real
     * shared IDXGIFactory vtable (the same one the game's factory uses, since
     * D3DMetal QI's all IDXGIFactoryN interfaces to one concrete vtable), then
     * patch slot 24 in place. We intentionally leak the factory. This is safe:
     * the earlier -1213 adapter failures were caused by a stale dcomp DLL
     * override, not by creating a factory. */
    Sleep(300);
    for (int i = 0; i < 400; i++) {
        HMODULE dx = GetModuleHandleA("dxgi.dll");
        if (!dx) dx = LoadLibraryA("dxgi.dll");
        if (dx) {
            PFN_Factory2 c2 = (PFN_Factory2)GetProcAddress(dx, "CreateDXGIFactory2");
            PFN_Factory  c1 = (PFN_Factory) GetProcAddress(dx, "CreateDXGIFactory");
            void *fac = NULL; HRESULT hr = E_FAIL;
            if (c2) hr = c2(0, &IID_IDXGIFactory2, &fac);
            if ((FAILED(hr) || !fac) && c1) hr = c1(&IID_IDXGIFactory2, &fac);
            if (SUCCEEDED(hr) && fac) {
                void **vt = *(void***)fac;   /* real shared factory vtable */
                if (patch_vtable(vt)) {
                    InterlockedExchange(&g_patched, 1);
                    shim_log("[shim] worker done (iter %d) factory=%p vtable=%p (leaked)",
                             i, fac, (void*)vt);
                    return 0;
                }
            } else if ((i % 10) == 0) {
                shim_log("[shim] worker CreateDXGIFactory hr=0x%08x (iter %d)", (unsigned)hr, i);
            }
        }
        Sleep(150);
    }
    shim_log("[shim] worker GAVE UP");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        shim_log("[shim] amd_ags proxy attached; starting worker");
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}

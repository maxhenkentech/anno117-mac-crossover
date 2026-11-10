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
#include <stdarg.h>
#include <tlhelp32.h>

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
    fprintf(f, "[%lu %lu] ", GetCurrentProcessId(), GetTickCount());
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


/* ---- diagnostics (issue #5: "launches once, then black screen + exit") ---- */

static void describe_addr(const void *addr, char *out, size_t n) {
    HMODULE m = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m) && m) {
        char path[MAX_PATH] = "?";
        GetModuleFileNameA(m, path, sizeof path);
        const char *base = strrchr(path, '\\');
        snprintf(out, n, "%s+0x%llx", base ? base + 1 : path,
                 (unsigned long long)((const char*)addr - (const char*)m));
    } else snprintf(out, n, "%p", addr);
}

static void log_modules(const char *why) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32 me; me.dwSize = sizeof me;
    shim_log("[diag] modules (%s):", why);
    if (Module32First(snap, &me)) do {
        shim_log("[diag]   %p %s", (void*)me.modBaseAddr, me.szExePath);
    } while (Module32Next(snap, &me));
    CloseHandle(snap);
}

static LONG WINAPI diag_veh(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    /* only fatal-looking first-chance exceptions; skip benign C++ exceptions (0xE06D7363) and noise */
    if (code == 0xC0000005 || code == 0xC000001D || code == 0xC0000094 ||
        code == 0xC00000FD || code == 0xC0000409 || code == 0xC0000374 ||
        code == 0xC0000096 || code == 0x80000003) {
        static volatile LONG n = 0;
        if (InterlockedIncrement(&n) > 40) return EXCEPTION_CONTINUE_SEARCH;
        char where[300];
        describe_addr(ep->ExceptionRecord->ExceptionAddress, where, sizeof where);
        shim_log("[diag] EXCEPTION 0x%08lx at %s info0=%llx info1=%llx tid=%lu",
                 code, where,
                 (unsigned long long)(ep->ExceptionRecord->NumberParameters > 0 ? ep->ExceptionRecord->ExceptionInformation[0] : 0),
                 (unsigned long long)(ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0),
                 GetCurrentThreadId());
        void *bt[24]; USHORT k = RtlCaptureStackBackTrace(0, 24, bt, NULL);
        for (USHORT i = 0; i < k; i++) {
            describe_addr(bt[i], where, sizeof where);
            shim_log("[diag]   #%u %s", i, where);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI diag_thread(LPVOID p) {
    (void)p;
    Sleep(8000);
    log_modules("t+8s");
    /* heartbeat so we can see how long the process lives */
    for (int i = 1; i <= 120; i++) {
        HWND w = find_game_window();
        char title[128] = "", cls[64] = ""; RECT r = {0};
        if (w) { GetWindowTextA(w, title, sizeof title); GetClassNameA(w, cls, sizeof cls); GetWindowRect(w, &r); }
        shim_log("[diag] heartbeat %d: top window=%p class='%s' title='%s' %ldx%ld",
                 i, (void*)w, cls, title, r.right - r.left, r.bottom - r.top);
        Sleep(2000);
    }
    return 0;
}

static void diag_init(void) {
    char exe[MAX_PATH] = "?"; GetModuleFileNameA(NULL, exe, sizeof exe);
    shim_log("[diag] attach: exe=%s", exe);
    shim_log("[diag] cmdline=%s", GetCommandLineA());
    AddVectoredExceptionHandler(1, diag_veh);
    HANDLE t = CreateThread(NULL, 0, diag_thread, NULL, 0, NULL);
    if (t) CloseHandle(t);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        shim_log("[shim] amd_ags proxy attached; starting worker");
        diag_init();
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    if (reason == DLL_PROCESS_DETACH)
        shim_log("[diag] detach (%s)", r ? "process terminating" : "FreeLibrary");
    return TRUE;
}

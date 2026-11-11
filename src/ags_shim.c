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

/* ---- diagnostics: snapshot of the swapchain's display (issue #5) ----
 * The issue-#5 crash happens ~0.2 s after the window switches to fullscreen.
 * Right after creating each swapchain we query (never hook) what DXGI reports
 * about it and the monitor it is on, so logs from crashing and working machines
 * can be compared. Note: do NOT patch the swapchain/output vtables -- D3DMetal's
 * swapchain methods dispatch back through their own vtable slots, so a patched
 * slot recurses into itself (v0.2.2 test build hung in ResizeBuffers). */

#define SHIM_VERSION "0.2.2-beta"

typedef struct { UINT Numerator, Denominator; } DXGI_RATIONAL_;
typedef struct {
    UINT Width, Height; DXGI_RATIONAL_ RefreshRate; UINT Format;
    int ScanlineOrdering, Scaling;
} DXGI_MODE_DESC_;
typedef struct {
    DXGI_MODE_DESC_ BufferDesc; DXGI_SAMPLE_DESC SampleDesc;
    UINT BufferUsage, BufferCount; HWND OutputWindow; BOOL Windowed;
    int SwapEffect; UINT Flags;
} DXGI_SWAP_CHAIN_DESC_;
typedef struct {
    WCHAR DeviceName[32]; RECT DesktopCoordinates; BOOL AttachedToDesktop;
    int Rotation; HMONITOR Monitor;
} DXGI_OUTPUT_DESC_;
typedef struct {
    WCHAR DeviceName[32]; RECT DesktopCoordinates; BOOL AttachedToDesktop;
    int Rotation; HMONITOR Monitor; UINT BitsPerColor; int ColorSpace;
    float RedPrimary[2], GreenPrimary[2], BluePrimary[2], WhitePoint[2];
    float MinLuminance, MaxLuminance, MaxFullFrameLuminance;
} DXGI_OUTPUT_DESC1_;

/* {068346e8-aaec-4b84-add7-137f513f77a1} */
static const GUID IID_IDXGIOutput6_ =
    {0x068346e8,0xaaec,0x4b84,{0xad,0xd7,0x13,0x7f,0x51,0x3f,0x77,0xa1}};

typedef HRESULT (WINAPI *PFN_QI)(void *This, REFIID riid, void **pp);
typedef HRESULT (WINAPI *PFN_ScGetDesc)(void*, DXGI_SWAP_CHAIN_DESC_*);
typedef HRESULT (WINAPI *PFN_GetOutput)(void*, void**);
typedef HRESULT (WINAPI *PFN_OutGetDesc)(void*, DXGI_OUTPUT_DESC_*);
typedef HRESULT (WINAPI *PFN_OutModeList)(void*, UINT, UINT, UINT*, void*);
typedef HRESULT (WINAPI *PFN_OutGetDesc1)(void*, DXGI_OUTPUT_DESC1_*);

#define VT(obj) (*(void***)(obj))

static void log_swapchain_snapshot(void *sc) {
    DXGI_SWAP_CHAIN_DESC_ d;
    HRESULT hr = ((PFN_ScGetDesc)VT(sc)[12])(sc, &d);              /* IDXGISwapChain::GetDesc */
    if (SUCCEEDED(hr))
        shim_log("[dxgi] swapchain %p: %ux%u fmt=%u windowed=%d buffers=%u flags=0x%x",
                 sc, d.BufferDesc.Width, d.BufferDesc.Height, d.BufferDesc.Format,
                 d.Windowed, d.BufferCount, d.Flags);
    void *out = NULL;
    hr = ((PFN_GetOutput)VT(sc)[15])(sc, &out);                      /* GetContainingOutput */
    if (FAILED(hr) || !out) {
        shim_log("[dxgi] swapchain %p: GetContainingOutput -> 0x%08x", sc, (unsigned)hr);
        return;
    }
    DXGI_OUTPUT_DESC_ od;
    if (SUCCEEDED(((PFN_OutGetDesc)VT(out)[7])(out, &od)))           /* IDXGIOutput::GetDesc */
        shim_log("[dxgi]   output %ls desktop=(%ld,%ld)-(%ld,%ld) attached=%d rotation=%d",
                 od.DeviceName, od.DesktopCoordinates.left, od.DesktopCoordinates.top,
                 od.DesktopCoordinates.right, od.DesktopCoordinates.bottom,
                 od.AttachedToDesktop, od.Rotation);
    UINT n28 = 0, n24 = 0;                                           /* GetDisplayModeList (count only) */
    HRESULT h28 = ((PFN_OutModeList)VT(out)[8])(out, 28, 0, &n28, NULL);
    HRESULT h24 = ((PFN_OutModeList)VT(out)[8])(out, 24, 0, &n24, NULL);
    shim_log("[dxgi]   display modes: rgba8=%u (0x%08x) rgb10a2=%u (0x%08x)",
             n28, (unsigned)h28, n24, (unsigned)h24);
    void *out6 = NULL;
    if (SUCCEEDED(((PFN_QI)VT(out)[0])(out, &IID_IDXGIOutput6_, &out6)) && out6) {
        DXGI_OUTPUT_DESC1_ d1;
        if (SUCCEEDED(((PFN_OutGetDesc1)VT(out6)[27])(out6, &d1)))   /* IDXGIOutput6::GetDesc1 */
            shim_log("[dxgi]   bpc=%u colorspace=%d luminance=%.1f..%.1f fullframe=%.1f",
                     d1.BitsPerColor, d1.ColorSpace, d1.MinLuminance,
                     d1.MaxLuminance, d1.MaxFullFrameLuminance);
        ((PFN_Release)VT(out6)[2])(out6);
    }
    ((PFN_Release)VT(out)[2])(out);
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
    if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain) log_swapchain_snapshot(*ppSwapChain);
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

/* Describe what a 64-bit value points at: a module, this thread's stack,
 * committed private/mapped memory, or nothing (and show it as ASCII if it is
 * printable -- issue #5 had a "pointer" that was really the text "ISteamNe"). */
static void describe_value(ULONG64 v, char *out, size_t n) {
    char ascii[9] = {0}; BOOL printable = TRUE;
    for (int i = 0; i < 8; i++) {
        unsigned char ch = (unsigned char)(v >> (8 * i));
        if (ch < 0x20 || ch > 0x7e) { printable = FALSE; break; }
        ascii[i] = (char)ch;
    }
    if (v < 0x10000) { snprintf(out, n, "small"); return; }
    NT_TIB *tib = (NT_TIB*)NtCurrentTeb();
    if (v >= (ULONG64)(ULONG_PTR)tib->StackLimit && v < (ULONG64)(ULONG_PTR)tib->StackBase) {
        snprintf(out, n, "stack"); return;
    }
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)(ULONG_PTR)v, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
        if (mbi.Type == MEM_IMAGE) { describe_addr((const void*)(ULONG_PTR)v, out, n); return; }
        snprintf(out, n, "%s prot=0x%lx", mbi.Type == MEM_MAPPED ? "mapped" : "heap/private", mbi.Protect);
        return;
    }
    snprintf(out, n, printable ? "UNMAPPED, ascii \"%s\"" : "UNMAPPED%s", printable ? ascii : "");
}

static BOOL readable(const void *p, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;
    const char *a = (const char*)p, *end = a + len;
    while (a < end) {
        if (!VirtualQuery(a, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return FALSE;
        a = (const char*)mbi.BaseAddress + mbi.RegionSize;
    }
    return TRUE;
}

static void dump_context(CONTEXT *c) {
    struct { const char *name; ULONG64 v; } regs[] = {
        {"rip", c->Rip}, {"rsp", c->Rsp}, {"rbp", c->Rbp},
        {"rax", c->Rax}, {"rbx", c->Rbx}, {"rcx", c->Rcx}, {"rdx", c->Rdx},
        {"rsi", c->Rsi}, {"rdi", c->Rdi}, {"r8",  c->R8},  {"r9",  c->R9},
        {"r10", c->R10}, {"r11", c->R11}, {"r12", c->R12}, {"r13", c->R13},
        {"r14", c->R14}, {"r15", c->R15},
    };
    char d[300];
    for (size_t i = 0; i < sizeof regs / sizeof regs[0]; i++) {
        describe_value(regs[i].v, d, sizeof d);
        shim_log("[crash]   %-3s = %016llx  %s", regs[i].name, (unsigned long long)regs[i].v, d);
    }
    const unsigned char *ip = (const unsigned char*)(ULONG_PTR)c->Rip;
    if (readable(ip - 16, 48)) {
        char hex[200]; int o = 0;
        for (int i = -16; i < 32; i++)
            o += snprintf(hex + o, sizeof hex - o, i == 0 ? "[%02x]" : "%02x ", ip[i]);
        shim_log("[crash]   bytes at rip-16: %s", hex);
    }
    const ULONG64 *sp = (const ULONG64*)(ULONG_PTR)c->Rsp;
    if (readable(sp, 16 * sizeof(ULONG64)))
        for (int i = 0; i < 16; i++) {
            describe_value(sp[i], d, sizeof d);
            shim_log("[crash]   [rsp+0x%02x] %016llx  %s", i * 8, (unsigned long long)sp[i], d);
        }
}

static LONG WINAPI diag_veh(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    /* only fatal-looking first-chance exceptions; skip benign C++ exceptions (0xE06D7363) and noise */
    if (code == 0xC0000005 || code == 0xC000001D || code == 0xC0000094 ||
        code == 0xC00000FD || code == 0xC0000409 || code == 0xC0000374 ||
        code == 0xC0000096 || code == 0x80000003) {
        static volatile LONG n = 0;
        LONG k_ex = InterlockedIncrement(&n);
        if (k_ex > 40) return EXCEPTION_CONTINUE_SEARCH;
        char where[300];
        describe_addr(ep->ExceptionRecord->ExceptionAddress, where, sizeof where);
        shim_log("[diag] EXCEPTION 0x%08lx at %s info0=%llx info1=%llx tid=%lu",
                 code, where,
                 (unsigned long long)(ep->ExceptionRecord->NumberParameters > 0 ? ep->ExceptionRecord->ExceptionInformation[0] : 0),
                 (unsigned long long)(ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0),
                 GetCurrentThreadId());
        if (ep->ExceptionRecord->NumberParameters > 1) {
            describe_value(ep->ExceptionRecord->ExceptionInformation[1], where, sizeof where);
            shim_log("[crash]   fault address: %s", where);
        }
        /* full register/stack dump for the first few only, to keep logs small */
        if (k_ex <= 3) dump_context(ep->ContextRecord);
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
    shim_log("[diag] shim %s attach: exe=%s", SHIM_VERSION, exe);
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

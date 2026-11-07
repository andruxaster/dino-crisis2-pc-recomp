/* dinput.dll proxy loaded by Dino2.exe from its own folder: forwards
 * DirectInputCreateA to the system dinput.dll and, on load, redirects the
 * original functions listed in hooks.c to their reimplementation (JMP rel32).
 *   dc2re.ini (optional, next to the exe):  off=Name1,Name2  disables hooks,
 *                                           off=*  disables all of them.
 *                                           trace=1 logs call counts every 5 s.
 *   dc2re.log: what was installed. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "dc2/types.h"
#include "hooks.h"

u8 *g_membase = (u8 *)IMAGE_BASE;

static FILE *g_log;
static char g_off[2048];
static volatile LONG g_calls[256];          /* per hook, counted by the thunk */
static int g_trace;

static int disabled(const char *name)
{
    if (!strcmp(g_off, "*")) return 1;
    const char *p = g_off; size_t n = strlen(name);
    while ((p = strstr(p, name))) {
        if ((p == g_off || p[-1] == ',') && (p[n] == 0 || p[n] == ',' || p[n] == '\r' || p[n] == '\n')) return 1;
        p += n;
    }
    return 0;
}

static void install(void)
{
    char ini[MAX_PATH]; GetModuleFileNameA(NULL, ini, MAX_PATH);
    char *s = strrchr(ini, '\\'); strcpy(s ? s + 1 : ini, "dc2re.ini");
    GetPrivateProfileStringA("hooks", "off", "", g_off, sizeof g_off, ini);
    g_trace = GetPrivateProfileIntA("hooks", "trace", 0, ini);
    strcpy(s ? s + 1 : ini, "dc2re.log");
    g_log = fopen(ini, "w");
    int n = 0;
    for (int i = 0; i < g_nhooks; i++) {
        const Hook *h = &g_hooks[i];
        if (disabled(h->name)) { if (g_log) fprintf(g_log, "skip    %08X %s\n", h->addr, h->name); continue; }
        u8 *p = (u8 *)(uintptr_t)h->addr; DWORD old;
        u8 *target = (u8 *)h->fn;
        if (g_trace && i < 256) {          /* thunk: inc dword [g_calls+4i]; jmp fn */
            static u8 *thunks;
            if (!thunks) thunks = VirtualAlloc(NULL, 4096 * 4, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            u8 *t = thunks + 16 * i;
            t[0] = 0xFF; t[1] = 0x05; *(u32 *)(t + 2) = (u32)(uintptr_t)&g_calls[i];
            t[6] = 0xE9; *(s32 *)(t + 7) = (s32)(target - (t + 11));
            target = t;
        }
        VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
        p[0] = 0xE9;
        *(s32 *)(p + 1) = (s32)(target - (p + 5));
        VirtualProtect(p, 5, old, &old);
        n++;
        if (g_log) fprintf(g_log, "hooked  %08X %s\n", h->addr, h->name);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    if (g_log) { fprintf(g_log, "%d of %d functions replaced\n", n, g_nhooks); fflush(g_log); }
}

/* trace=1: every 5 s, how often each replaced function ran */
static DWORD WINAPI trace_thread(void *arg)
{
    (void)arg;
    for (int sec = 5;; sec += 5) {
        Sleep(5000);
        if (!g_log) continue;
        fprintf(g_log, "[%3d s]", sec);
        for (int i = 0; i < g_nhooks && i < 256; i++)
            if (g_calls[i]) fprintf(g_log, " %s=%ld", g_hooks[i].name, g_calls[i]);
        fprintf(g_log, "\n"); fflush(g_log);
    }
    return 0;
}

typedef HRESULT (WINAPI *DICreateA)(HINSTANCE, DWORD, void **, void *);

__declspec(dllexport) HRESULT WINAPI DirectInputCreateA(HINSTANCE h, DWORD v, void **out, void *unk)
{
    static DICreateA real;
    if (!real) {
        char path[MAX_PATH]; GetSystemDirectoryA(path, MAX_PATH); strcat(path, "\\dinput.dll");
        HMODULE m = LoadLibraryA(path);
        real = m ? (DICreateA)GetProcAddress(m, "DirectInputCreateA") : NULL;
        if (!real) return E_FAIL;
    }
    return real(h, v, out, unk);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID res)
{
    (void)inst; (void)res;
    if (reason == DLL_PROCESS_ATTACH) {
        install();
        if (g_trace) CreateThread(NULL, 0, trace_thread, NULL, 0, NULL);
    }
    if (reason == DLL_PROCESS_DETACH && g_log) fclose(g_log);
    return TRUE;
}

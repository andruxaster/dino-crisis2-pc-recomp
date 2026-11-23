/* The PC renderer (include/dc2/render.h) on OpenGL 1.4 + SDL2: the PC layer's
 * pre-transformed Direct3D vertices drawn with the fixed-function pipeline.
 *   vertices: clip space x/rhw.. so that texturing stays perspective-correct;
 *   flat shading: the first vertex's colour (D3D), via glProvokingVertex;
 *   vertex fog: the specular's alpha as the fog coordinate (1 - factor);
 *   textures: by surface id, uploaded again when their version changes. */
#include <SDL.h>                     /* (SDL's own way: its include/SDL2 on the path, as every SDL2 package sets it) */
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dc2/render.h"
#include "render_gl.h"

/* past OpenGL 1.1 (Windows' opengl32 exports no more): looked up at start */
static PFNGLFOGCOORDFPROC p_glFogCoordf;
static PFNGLPROVOKINGVERTEXPROC p_glProvokingVertex;

static SDL_Window *g_win;
static SDL_GLContext g_ctx;
static int g_w, g_h;

typedef struct { u32 id, version; GLuint tex; } TexEntry;
#define NTEX 4096
static TexEntry g_tex[NTEX];
static RenderGlStats g_stats;

static TexEntry *tex_slot(u32 id)
{
    TexEntry *e = &g_tex[id % NTEX];
    if (e->id != id && e->tex) { glDeleteTextures(1, &e->tex); e->tex = 0; }
    e->id = id;
    return e;
}

static void bind(const RTexture *t, int filter)
{
    if (!t) { glDisable(GL_TEXTURE_2D); return; }
    TexEntry *e = tex_slot(t->id);
    if (!e->tex) { glGenTextures(1, &e->tex); e->version = t->version + 1; }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, e->tex);
    if (e->version != t->version) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(t->pitch / t->bpp));
        if (t->bpp == 2) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, (GLsizei)t->w, (GLsizei)t->h, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, t->pixels);
        else glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)t->w, (GLsizei)t->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, t->pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        e->version = t->version;
        g_stats.uploads++;
    }
    GLint f = filter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
}

static GLenum blend_factor(u8 b)
{
    static const GLenum k[] = {GL_ONE, GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                               GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA_SATURATE};
    return b < sizeof k / sizeof k[0] ? k[b] : GL_ONE;
}
static GLenum cmp(u8 f) { return f >= 1 && f <= 8 ? GL_NEVER + (f - 1) : GL_ALWAYS; }

static void apply(const RState *s, const RTexture *t)
{
    if (s->blend) { glEnable(GL_BLEND); glBlendFunc(blend_factor(s->src), blend_factor(s->dst)); } else glDisable(GL_BLEND);
    if (s->alpha_test) { glEnable(GL_ALPHA_TEST); glAlphaFunc(cmp(s->alpha_func), s->alpha_ref / 255.0f); } else glDisable(GL_ALPHA_TEST);
    if (s->z_test) { glEnable(GL_DEPTH_TEST); glDepthFunc(cmp(s->z_func)); } else glDisable(GL_DEPTH_TEST);
    glDepthMask(s->z_write ? GL_TRUE : GL_FALSE);
    glShadeModel(s->flat ? GL_FLAT : GL_SMOOTH);
    bind(t, s->filter);
    if (t && s->modulate2x) {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE); glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
    } else if (t) {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
    }
    if (s->fog) {
        glEnable(GL_FOG);
        float c[4] = {(s->fog_color >> 16 & 0xFF) / 255.0f, (s->fog_color >> 8 & 0xFF) / 255.0f, (s->fog_color & 0xFF) / 255.0f, 1};
        glFogfv(GL_FOG_COLOR, c);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        if (s->fog_table == 0) { glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD); glFogf(GL_FOG_START, 0); glFogf(GL_FOG_END, 1); }
        else { glFogi(GL_FOG_COORD_SRC, GL_FRAGMENT_DEPTH); glFogf(GL_FOG_START, s->fog_start); glFogf(GL_FOG_END, s->fog_end); }
    } else glDisable(GL_FOG);
}

static void vertex(const RVertex *v)
{
    float w = v->rhw > 0 ? 1.0f / v->rhw : 1.0f;
    glColor4ub((GLubyte)(v->diffuse >> 16), (GLubyte)(v->diffuse >> 8), (GLubyte)v->diffuse, (GLubyte)(v->diffuse >> 24));
    if (p_glFogCoordf) p_glFogCoordf(1.0f - (v->specular >> 24) / 255.0f);
    glTexCoord2f(v->u, v->v);
    float x = v->x / (float)g_w * 2.0f - 1.0f, y = 1.0f - v->y / (float)g_h * 2.0f, z = v->z * 2.0f - 1.0f;
    glVertex4f(x * w, y * w, z * w, w);
}

static void gl_draw(int prim, const RVertex *v, int n, const RState *s, const RTexture *t)
{
    static const GLenum k[] = {0, GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
    if (prim < 1 || prim > 6 || n <= 0) return;
    apply(s, t);
    if (g_stats.log)
        printf("  draw %d x%d blend %d %d/%d atest %d z %d/%d/%d fog %d/%d tex %u %ux%u v0 %.1f,%.1f z %.4f rhw %.4f c %08X s %08X uv %.3f,%.3f\n",
               prim, n, s->blend, s->src, s->dst, s->alpha_test, s->z_test, s->z_write, s->z_func, s->fog, s->fog_table, t ? t->id : 0,
               t ? t->w : 0, t ? t->h : 0, v->x, v->y, v->z, v->rhw, v->diffuse, v->specular, v->u, v->v);
    glBegin(k[prim]);
    for (int i = 0; i < n; i++) vertex(&v[i]);
    glEnd();
    g_stats.draws++; g_stats.verts += (u32)n;
}

static void gl_clear(int target, int zbuffer, u32 rgb)
{
    GLbitfield m = 0;
    if (target) { glClearColor((rgb >> 16 & 0xFF) / 255.0f, (rgb >> 8 & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, 1); m |= GL_COLOR_BUFFER_BIT; }
    if (zbuffer) { glDepthMask(GL_TRUE); glClearDepth(1.0); m |= GL_DEPTH_BUFFER_BIT; }
    if (m) glClear(m);
}

static void gl_begin_frame(void) {}

static void gl_present(void)
{
    if (g_stats.on_present) g_stats.on_present(&g_stats);
    SDL_GL_SwapWindow(g_win);
    g_stats.frames++; g_stats.draws = g_stats.verts = 0;
}

static int g_vx, g_vy, g_vw, g_vh;     /* the picture's place in the window */

/* the picture back as 640x480-ish 1555 (scaled windows and fullscreen sampled down) */
static void gl_read_back(u16 *dst, u32 w, u32 h, u32 pitch)
{
    if ((u32)g_vw == w && (u32)g_vh == h) {
        glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(pitch / 2));
        glReadPixels(g_vx, g_vy, (GLsizei)w, (GLsizei)h, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, dst);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        u16 row[2048];
        for (u32 y = 0; y < h / 2 && w <= 2048; y++) {      /* GL's rows bottom-up */
            u16 *a = (u16 *)((u8 *)dst + y * pitch), *b = (u16 *)((u8 *)dst + (h - 1 - y) * pitch);
            memcpy(row, a, w * 2); memcpy(a, b, w * 2); memcpy(b, row, w * 2);
        }
        return;
    }
    static u8 *big; static size_t cap;
    size_t need = (size_t)g_vw * g_vh * 4;
    if (cap < need) { free(big); big = malloc(need); cap = big ? need : 0; if (!big) return; }
    glReadPixels(g_vx, g_vy, g_vw, g_vh, GL_BGRA, GL_UNSIGNED_BYTE, big);
    for (u32 y = 0; y < h; y++) {
        const u8 *src = big + (size_t)(g_vh - 1 - (int)(y * (u32)g_vh / h)) * g_vw * 4;
        u16 *d = (u16 *)((u8 *)dst + y * pitch);
        for (u32 x = 0; x < w; x++) {
            const u8 *p = src + (size_t)(x * (u32)g_vw / w) * 4;     /* B, G, R, A */
            d[x] = (u16)(0x8000 | (p[2] >> 3) << 10 | (p[1] >> 3) << 5 | p[0] >> 3);
        }
    }
}

static void gl_texture_freed(u32 id)
{
    TexEntry *e = &g_tex[id % NTEX];
    if (e->id == id && e->tex) { glDeleteTextures(1, &e->tex); e->tex = 0; e->id = 0; }
}

static const Renderer k_gl = {gl_begin_frame, gl_clear, gl_draw, gl_present, gl_read_back, gl_texture_freed};

static void pads_open(void);

/* the picture's place in the drawable, kept 4:3 (black bars) */
static void layout(void)
{
    int dw = g_w, dh = g_h, w = g_w, h = g_h;
    SDL_GL_GetDrawableSize(g_win, &dw, &dh);
    int vw = dw, vh = dh;
    if (vw * h > vh * w) vw = vh * w / h; else vh = vw * h / w;
    glViewport(0, 0, dw, dh);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    g_vx = (dw - vw) / 2; g_vy = (dh - vh) / 2; g_vw = vw; g_vh = vh;
    glViewport(g_vx, g_vy, vw, vh);
}

void RenderGl_SetMode(int w, int h, int fullscreen)
{
    if (!g_win) return;
    if (fullscreen) SDL_SetWindowFullscreen(g_win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    else {
        SDL_SetWindowFullscreen(g_win, 0);
        SDL_SetWindowSize(g_win, w, h);
        SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_ShowCursor(fullscreen ? SDL_DISABLE : SDL_ENABLE);
    layout();
}

/* windowed at w x h times scale, or fullscreen on the desktop's mode with the
   picture kept 4:3 (black bars) */
const Renderer *RenderGl_Open(int w, int h, int scale, int fullscreen)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    pads_open();
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (scale < 1) scale = 1;
    g_win = SDL_CreateWindow("Dino Crisis 2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w * scale, h * scale,
                             SDL_WINDOW_OPENGL | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!g_win) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    g_ctx = SDL_GL_CreateContext(g_win);
    if (!g_ctx) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetSwapInterval(0);
    g_w = w; g_h = h;
    layout();
    if (fullscreen) SDL_ShowCursor(SDL_DISABLE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    p_glFogCoordf = (PFNGLFOGCOORDFPROC)SDL_GL_GetProcAddress("glFogCoordf");
    p_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC)SDL_GL_GetProcAddress("glProvokingVertex");
    if (p_glProvokingVertex) p_glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    glDisable(GL_CULL_FACE);
    return &k_gl;
}

RenderGlStats *RenderGl_Stats(void) { return &g_stats; }

/* ---- game controllers (SDL's GameController: DualSense, DualShock 4, Xbox,
 * Switch Pro and others SDL knows; DualSense and DualShock 4 by HIDAPI, over
 * USB or Bluetooth). Buttons as the Dreamcast port's pad (port/dc/main.c),
 * by position: south (Cross, A) confirm 0x20, east (Circle, B) back 0x40,
 * west (Square, X) 0x10, north (Triangle, Y) 0x100, L1/R1 0x04/0x08,
 * L2/R2 0x02/0x01, Options/Start 0x800, Create/Back the map (0x01). The
 * d-pad or the left stick move. All connected pads count. */
#define NPADS 4
static SDL_GameController *g_pads[NPADS];

static void pad_add(int index)
{
    if (!SDL_IsGameController(index)) {
        fprintf(stderr, "pad %d: \"%s\" is not a game controller SDL knows (no mapping)\n", index, SDL_JoystickNameForIndex(index));
        return;
    }
    for (int i = 0; i < NPADS; i++)
        if (!g_pads[i]) {
            g_pads[i] = SDL_GameControllerOpen(index);
            if (g_pads[i]) fprintf(stderr, "pad: %s\n", SDL_GameControllerName(g_pads[i]));
            else fprintf(stderr, "pad %d: %s\n", index, SDL_GetError());
            return;
        }
}

static void pad_remove(SDL_JoystickID id)
{
    for (int i = 0; i < NPADS; i++)
        if (g_pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pads[i])) == id) {
            SDL_GameControllerClose(g_pads[i]);
            g_pads[i] = NULL;
        }
}

static void pads_open(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");         /* (DualSense: SDL 2.0.14 on) */
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL pads: %s\n", SDL_GetError()); return; }
    /* (connected ones come as SDL_CONTROLLERDEVICEADDED events too, at the first poll) */
}

static u16 pads_read(void)
{
    u16 p = 0;
    for (int i = 0; i < NPADS; i++) {
        SDL_GameController *c = g_pads[i];
        if (!c) continue;
#define B(b) SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_##b)
        int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
        if (B(DPAD_UP) || ly < -16000) p |= 0x1000;
        if (B(DPAD_DOWN) || ly > 16000) p |= 0x4000;
        if (B(DPAD_LEFT) || lx < -16000) p |= 0x8000;
        if (B(DPAD_RIGHT) || lx > 16000) p |= 0x2000;
        if (B(A)) p |= 0x20;
        if (B(B)) p |= 0x40;
        if (B(X)) p |= 0x10;
        if (B(Y)) p |= 0x100;
        if (B(LEFTSHOULDER)) p |= 0x04;
        if (B(RIGHTSHOULDER)) p |= 0x08;
        if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) p |= 0x02;
        if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) p |= 0x01;
        if (B(START)) p |= 0x800;
        if (B(BACK)) p |= 0x01;
#if SDL_VERSION_ATLEAST(2, 0, 14)
        if (B(TOUCHPAD)) p |= 0x01;
#endif
#undef B
    }
    return p;
}

/* the pad from the keyboard: arrows, Z (confirm, 0x820), X (back, 0x40), C (action, 0x20), Enter (start 0x8);
   and from the game controllers (above) */
u16 RenderGl_Pad(int *quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) *quit = 1;
        else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) layout();   /* (full screen entered or left) */
        else if (e.type == SDL_CONTROLLERDEVICEADDED) pad_add(e.cdevice.which);
        else if (e.type == SDL_CONTROLLERDEVICEREMOVED) pad_remove(e.cdevice.which);
    }
    const Uint8 *k = SDL_GetKeyboardState(0);
    u16 p = pads_read();
    if (k[SDL_SCANCODE_UP]) p |= 0x1000;
    if (k[SDL_SCANCODE_DOWN]) p |= 0x4000;
    if (k[SDL_SCANCODE_LEFT]) p |= 0x8000;
    if (k[SDL_SCANCODE_RIGHT]) p |= 0x2000;
    if (k[SDL_SCANCODE_Z]) p |= 0x820;
    if (k[SDL_SCANCODE_X]) p |= 0x40;
    if (k[SDL_SCANCODE_C]) p |= 0x20;
    if (k[SDL_SCANCODE_RETURN]) p |= 0x8;
    if (k[SDL_SCANCODE_ESCAPE]) *quit = 1;
    return p;
}

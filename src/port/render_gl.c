/* The PC renderer (include/dc2/render.h) on OpenGL 1.4 + SDL2: the PC layer's
 * pre-transformed Direct3D vertices drawn with the fixed-function pipeline.
 *   vertices: clip space x/rhw.. so that texturing stays perspective-correct;
 *   flat shading: the first vertex's colour (D3D), via glProvokingVertex;
 *   vertex fog: the specular's alpha as the fog coordinate (1 - factor);
 *   textures: by surface id, uploaded again when their version changes. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <string.h>
#include "dc2/render.h"
#include "render_gl.h"

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
    glFogCoordf(1.0f - (v->specular >> 24) / 255.0f);
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

static void gl_read_back(u16 *dst, u32 w, u32 h, u32 pitch)
{
    glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(pitch / 2));
    glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, dst);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    u16 row[2048];
    for (u32 y = 0; y < h / 2 && w <= 2048; y++) {          /* GL's rows bottom-up */
        u16 *a = (u16 *)((u8 *)dst + y * pitch), *b = (u16 *)((u8 *)dst + (h - 1 - y) * pitch);
        memcpy(row, a, w * 2); memcpy(a, b, w * 2); memcpy(b, row, w * 2);
    }
}

static void gl_texture_freed(u32 id)
{
    TexEntry *e = &g_tex[id % NTEX];
    if (e->id == id && e->tex) { glDeleteTextures(1, &e->tex); e->tex = 0; e->id = 0; }
}

static const Renderer k_gl = {gl_begin_frame, gl_clear, gl_draw, gl_present, gl_read_back, gl_texture_freed};

const Renderer *RenderGl_Open(int w, int h, int scale)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    g_win = SDL_CreateWindow("Dino Crisis 2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w * scale, h * scale, SDL_WINDOW_OPENGL);
    if (!g_win) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    g_ctx = SDL_GL_CreateContext(g_win);
    if (!g_ctx) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetSwapInterval(0);
    g_w = w; g_h = h;
    glViewport(0, 0, w * scale, h * scale);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    glDisable(GL_CULL_FACE);
    return &k_gl;
}

RenderGlStats *RenderGl_Stats(void) { return &g_stats; }

/* the pad from the keyboard: arrows, Z (confirm, 0x820), X (back, 0x40), C (action, 0x20), Enter (start 0x8) */
u16 RenderGl_Pad(int *quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) *quit = 1;
    const Uint8 *k = SDL_GetKeyboardState(0);
    u16 p = 0;
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

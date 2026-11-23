/* File loading (0x403E40..0x404934): a request names a file of the table
 * 0x71B230; File_Process reads "data\NAME" in one go and hands each 32-byte
 * record of its 0x800-byte header to the handler of the record's type.
 * Record: u32 type, u32 size, u32 arg (destination / slot), u32 arg2, ...;
 * data is sector (0x800) aligned after the header. */
#include "dc2/game.h"
#include "dc2/pcdraw.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"

#define A_FILE_REC    0x87FFE0u   /* u32 -> current record */
#define A_FILE_LEFT   0x87FFE4u   /* u32 bytes left */
#define A_FILE_ID     0x87FFE8u   /* u16 */
#define A_FILE_STATE  0x87FFEAu   /* u16: 0 idle, 1 load pending */
#define A_FILE_INDEX  0x87FFECu   /* u16 entry of an archive */
#define A_FILE_FLAGS  0x87FFEEu   /* u16: 1 has header, 2/4/8 archive of 5/33/7 sectors */
#define A_FILE_NAMES  0x71B230u   /* char *[676] */
#define A_FILE_COUNT  0x87DC9Cu
#define FILE_HDR      0x6E6D00u   /* the header is read here */
#define A_SAVED_VRAM  0x746CA0u   /* buffer for a VRAM block kept across a load */
#define A_SND_BANKS   0x87FBE0u   /* 8 bytes per slot: data, descriptors */
#define A_SND_STREAMS 0x87FB80u   /* 20 bytes per slot: +0 handle, +4 data */
#define A_PC_BUFFER   0x87F944u
#define UNPACK_BUF    0x5A0000u

u8 DC2CALL Rand8(void)                                              /* 0x48F750 */
{
    u16 v = U16(0x87DE10);
    u8 hi = (u8)((v * 3u) >> 8);
    u16 n = (u16)(hi << 8 | (u8)(v + hi));
    if (n == v) n += 0x101;
    U16(0x87DE10) = n;
    return (u8)n;
}

u8 DC2CALL File_RequestLoad(u32 id)                                 /* 0x403E50 */
{
    if (U16(A_FILE_STATE)) return 0;
    U32(A_FILE_REC) = FILE_HDR;
    U32(A_FILE_COUNT)++;
    U16(A_FILE_ID) = (u16)id;
    U16(A_FILE_FLAGS) |= 1;
    U16(A_FILE_STATE) = 1;
    return 1;
}

/* load a headerless file whole to dest */
u8 DC2CALL File_RequestRaw(u32 id, gptr dest)                       /* 0x403E90 */
{
    if (!File_RequestLoad(id)) return 0;
    U16(A_FILE_FLAGS) = 0;
    U32(A_FILE_REC) = FILE_HDR;
    U32(FILE_HDR) = 0;                          /* type 0: copy */
    U32(U32(A_FILE_REC) + 8) = dest;
    U32(U32(A_FILE_REC) + 0x20) = 0x6D6D7564;   /* "dumm": end */
    U32(A_FILE_REC) = FILE_HDR;
    return 1;
}

/* an archive entry (file id, entry, bytes, flags 2/4/8) by its records
   {type, size, x, y, w, h} (VRAM rectangles; type 2 the CLUT), "dumm" after
   them. The records' last 16 bytes are left as they were (the original
   copies stack garbage there). */
static u8 load_entry(u32 id, u32 idx, u32 left, u16 flags, const u32 (*rec)[6], u32 n)
{
    if (!File_RequestLoad(id)) return 0;
    U16(A_FILE_INDEX) = (u16)idx;
    U32(A_FILE_LEFT) = left;
    U16(A_FILE_FLAGS) = flags;
    gptr r = FILE_HDR;
    for (u32 k = 0; k < n; k++, r += 0x20) {
        U32(r) = rec[k][0]; U32(r + 4) = rec[k][1];
        for (u32 j = 0; j < 4; j++) U16(r + 8 + 2 * j) = (u16)rec[k][2 + j];
    }
    U32(r) = 0x6D6D7564;
    U32(A_FILE_REC) = FILE_HDR;
    return 1;
}

/* ITEM.DAT entry: a 128x64 8bpp icon -> VRAM 768,192, its CLUT row 497 */
u8 DC2CALL Item_LoadIcon(u32 idx)                                   /* 0x403EE0 */
{
    static const u32 rec[2][6] = {{1, 0x2000, 0x300, 0xC0, 0x40, 0x40}, {2, 0x200, 0, 0x1F1, 0x100, 1}};
    return load_entry(0x79, idx, 0x2800, 2, rec, 2);
}

/* MAP.DAT entry: two 256x256 4bpp pages -> VRAM 448,0 and 384,0, CLUT row 500 */
u8 DC2CALL Map_LoadPage(u32 idx)                                    /* 0x403FD0 */
{
    static const u32 rec[3][6] = {{1, 0x8000, 0x1C0, 0, 0x40, 0x100}, {1, 0x8000, 0x180, 0, 0x40, 0x100}, {2, 0x200, 0, 0x1F4, 0x10, 1}};
    return load_entry(0x8A, idx, 0x10800, 4, rec, 3);
}

/* FILE.DAT entry: a 128x96 8bpp picture -> VRAM 384,256, CLUT row 497 */
u8 DC2CALL File_LoadPicture(u32 idx)                                /* 0x404100 */
{
    static const u32 rec[2][6] = {{1, 0x3000, 0x180, 0x100, 0x40, 0x60}, {2, 0x200, 0, 0x1F1, 0x100, 1}};
    return load_entry(0x77, idx, 0x3800, 8, rec, 2);
}

/* ---- record handlers ------------------------------------------------------------ */
void DC2CALL File_Copy(gptr rec, gptr src)                          /* 0x4044C0  type 0 */
{
    memmove(MEM(U32(rec + 8)), MEM(src), U32(rec + 4));
}

/* upload 32x32 tiles of 0x800 bytes, row by row, into the rectangle of the record */
static void upload_tiles(gptr rec, gptr r, gptr src, u32 tiles)
{
    u16 x = 0, y = 0;
    while (tiles--) {
        s16 h = (s16)(U16(rec + 0xE) - y);
        U16(r + 6) = (u16)(h > 0x20 ? 0x20 : h);
        U16(r) = (u16)(x + U16(rec + 8));
        U16(r + 2) = (u16)(y + U16(rec + 0xA));
        Gpu_LoadImage(r, src);
        x += 0x20; src += 0x800;
        if (x >= U16(rec + 0xC)) { x = 0; y += 0x20; }
    }
}

void DC2CALL File_Image(gptr rec, gptr src)                         /* 0x4044F0  type 1 */
{
    gptr r = Tmp_Push(8);
    int keep = 0;
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x20; U16(r + 6) = 0x20;
    if (U16(rec + 0xA) == 0x280) {              /* keep VRAM (0x340,0x80) 64x128 for later */
        U16(rec + 0xA) = 0x80;
        U32(A_SAVED_VRAM) = Crt_malloc(U32(rec + 4));
        keep = 1;
    }
    if ((s32)(U32(rec + 4) & ~0x7FFu) > 0) {
        u32 n = 0;
        u16 x = 0, y = 0;
        gptr s = src;
        do {
            s16 h = (s16)(U16(rec + 0xE) - y);
            U16(r + 6) = (u16)(h > 0x20 ? 0x20 : h);
            U16(r) = (u16)(x + U16(rec + 8));
            U16(r + 2) = (u16)(y + U16(rec + 0xA));
            Gpu_LoadImage(r, s);
            x += 0x20; s += 0x800;
            if (x >= U16(rec + 0xC)) { x = 0; y += 0x20; }
            n = (u8)(n + 1);
        } while ((s32)n < (s32)U32(rec + 4) >> 11);
    }
    gptr buf = U32(A_SAVED_VRAM);
    U16(r) = 0x340; U16(r + 2) = 0x80; U16(r + 4) = 0x40; U16(r + 6) = 0x80;
    if (keep) {
        Gpu_StoreImage(r, buf);
    } else if (buf && U16(rec + 8) == 0x300 && U16(rec + 0xA) == 0 &&
               U16(rec + 0xC) == 0x80 && U16(rec + 0xE) == 0x100) {
        Gpu_LoadImage(r, buf);                  /* put the kept block back */
        Crt_free(U32(A_SAVED_VRAM));
        U32(A_SAVED_VRAM) = 0;
    }
    Tmp_Pop(8);
}

void DC2CALL File_Clut(gptr rec, gptr src)                          /* 0x404680  type 2 */
{
    gptr r = Tmp_Push(8);
    U16(r) = U16(rec + 8); U16(r + 2) = U16(rec + 0xA);
    U16(r + 4) = U16(rec + 0xC); U16(r + 6) = U16(rec + 0xE);
    Gpu_LoadImageIfChanged(r, src);
    Tmp_Pop(8);
}

void DC2CALL File_SoundBank(gptr rec, gptr src)                     /* 0x4046C0  type 3 */
{
    Snd_FreeBank(U32(rec + 8));
    gptr m = Crt_malloc(U32(rec + 4));
    U32(A_SND_BANKS + U32(rec + 8) * 8) = m;
    U32(A_SND_BANKS + U32(rec + 8) * 8 + 4) = U32(A_SND_BANKS + U32(rec + 8) * 8) + 0x100;
    memmove(MEM(U32(A_SND_BANKS + U32(rec + 8) * 8)), MEM(src), U32(rec + 4));
    for (u32 i = 0; i < 0x100; i += 8) {        /* 32 sounds {offset, buffer} */
        gptr slot = A_SND_BANKS + U32(rec + 8) * 8;
        u32 off = U32(U32(slot + 4) + i);
        if (!off) continue;
        u32 b = Snd_CreateBuffer(U32(slot) + off, 0);
        U32(U32(A_SND_BANKS + U32(rec + 8) * 8 + 4) + i + 4) = b;
    }
}

void DC2CALL File_Stream(gptr rec, gptr src)                        /* 0x404770  type 4 */
{
    Snd_FreeStream(U32(rec + 8));
    gptr m = Crt_malloc(U32(rec + 4));
    U32(A_SND_STREAMS + 4 + U32(rec + 8) * 20) = m;
    memmove(MEM(U32(A_SND_STREAMS + 4 + U32(rec + 8) * 20)), MEM(src), U32(rec + 4));
    u32 h = Strm_Open(U32(A_SND_STREAMS + 4 + U32(rec + 8) * 20), U32(rec + 4), U32(rec + 0xC));
    U32(A_SND_STREAMS + U32(rec + 8) * 20) = h;
}

void DC2CALL File_Unpack(gptr rec, gptr src)                        /* 0x4047F0  type 5 */
{
    Lzss_Decode(U32(rec + 8), src, (s32)U32(rec + 4));
}

void DC2CALL File_Texture(gptr rec, gptr src)                       /* 0x404810  type 6 */
{
    gptr r = Tmp_Push(8);
    U16(r + 4) = 0x20; U16(r + 6) = 0x20;
    U16(r) = 0; U16(r + 2) = 0;
    Lzss_Decode(UNPACK_BUF, src, (s32)U32(rec + 4));
    u32 n = (u32)((U16(rec + 0xE) + 0x1F) & 0xFFE0) * ((U16(rec + 0xC) + 0x1F) & 0xFFE0) * 2;
    if (n) upload_tiles(rec, r, UNPACK_BUF, (n + 0x7FF) >> 11);
    Tmp_Pop(8);
}

void DC2CALL File_PcBuffer(gptr rec, gptr src)                      /* 0x4048F0  type 7 */
{
    if (U32(A_PC_BUFFER)) return;
    U32(A_PC_BUFFER) = Crt_malloc(U32(rec + 4));
    memmove(MEM(U32(A_PC_BUFFER)), MEM(src), U32(rec + 4));
    PcDraw_SetFont(U32(A_PC_BUFFER));
}

static void (DC2CALL *const g_file_handlers[8])(gptr, gptr) = {
    File_Copy, File_Image, File_Clut, File_SoundBank, File_Stream, File_Unpack, File_Texture, File_PcBuffer,
};

void DC2CALL File_Process(void)                                     /* 0x404290 */
{
    if (g_gog_fixes) {                              /* GOG: fog off for file 0x78; two values cleared for file 0xEC */
        u8 f = U8(A_FILE_ID);
        if (f == 0x78) { PcDraw_SetFogEnable(0); Light_SetFar(0); }
        if (f == 0xEC) { U32(0x746CE6) = 0; U32(0x74B52E) = 0; }
    }
    gptr name = U32(A_FILE_NAMES + U16(A_FILE_ID) * 4);
    if (!name) return;
    gptr path = Tmp_Push(0x40);
    U32(A_FILE_REC) = FILE_HDR;
    memcpy(MEM(path), "data\\", 5);
    for (u32 i = 0; i < 0x3A; i++) if (!(U8(path + 5 + i) = U8(name + i))) break;
    U8(path + 0x3F) = 0;
    s32 fd = Pc_Open(path, 0, 0);
    Tmp_Pop(0x40);
    if (fd == -1) return;
    u8 fl = U8(A_FILE_FLAGS);
    if (fl & 1) {                                  /* container: header, then the records */
        U32(A_FILE_LEFT) = Pc_Size(fd);
        Pc_Read(fd, FILE_HDR, 0x800);
        U32(A_FILE_LEFT) -= 0x800;
    } else if (fl & 2) {
        Pc_Seek(fd, (s32)(U16(A_FILE_INDEX) * 5u << 11), 1);
    } else if (fl & 4) {
        Pc_Seek(fd, (s32)(U16(A_FILE_INDEX) * 33u << 11), 1);
    } else if (fl & 8) {
        Pc_Seek(fd, (s32)(U16(A_FILE_INDEX) * 7u << 11), 1);
    } else {                                       /* one record set up by the caller */
        U32(A_FILE_LEFT) = Pc_Size(fd);
        U32(U32(A_FILE_REC) + 4) = U32(A_FILE_LEFT);
        U32(A_FILE_LEFT) = (U32(U32(A_FILE_REC) + 4) + 0x7FF) & ~0x7FFu;
    }
#if defined(DC2_LOWMEM) && !defined(DC2_IN_PROCESS)
    /* small-memory builds (the Dreamcast): a container is read record by
       record rather than whole, and the music streams' data (type 4, the
       MP3 of the room's music, several MB) is not read: the stream plays
       from data\BGM\ instead (IMA ADPCM made by tools/dreamcast/dc2_music.py,
       read from the disc as it plays; none there: no handle, no music) */
    if (fl & 1) {
        /* (a header-only MS_*.DAT has no data left: its stream records still open) */
        while (U32(A_FILE_LEFT) || (U32(U32(A_FILE_REC)) == 4 && U32(A_FILE_REC) < FILE_HDR + 0x800)) {
            gptr rec = U32(A_FILE_REC);
            u32 step = (U32(rec + 4) + 0x7FF) & ~0x7FFu;
            if (step > U32(A_FILE_LEFT) && U32(rec) != 4) step = U32(A_FILE_LEFT);
            if (U32(rec) == 4) {                   /* played from data\BGM (dc2_music.py) */
                Snd_FreeStream(U32(rec + 8));
                u32 h = Strm_OpenFile((const char *)MEM(U32(A_FILE_NAMES + U16(A_FILE_ID) * 4)), (rec - FILE_HDR) / 0x20, U32(rec + 0xC));
                U32(A_SND_STREAMS + U32(rec + 8) * 20) = h;
                /* the slot's data: Strm_Start plays only a slot that has some,
                   Snd_FreeStream frees it; a small stand-in when the stream opened */
                U32(A_SND_STREAMS + 4 + U32(rec + 8) * 20) = h != 0xFFFFFFFF ? Crt_malloc(16) : 0;
                if (step <= U32(A_FILE_LEFT)) Pc_Seek(fd, (s32)step, 1);
                else step = U32(A_FILE_LEFT);       /* (MS_*.DAT cut down to their header) */
            } else {
                gptr b = Crt_malloc(step ? step : 16);
                if (!b) break;
                Pc_Read(fd, b, step);
                if (U32(rec) <= 7) { g_file_handlers[U32(rec)](rec, b); rec = U32(A_FILE_REC); }
                Crt_free(b);
                step = (U32(rec + 4) + 0x7FF) & ~0x7FFu;
                if (step > U32(A_FILE_LEFT)) step = U32(A_FILE_LEFT);
            }
            U32(A_FILE_REC) = rec + 0x20;
            U32(A_FILE_LEFT) -= step;
        }
        Pc_Close(fd);
        U16(A_FILE_STATE) = 0;
        return;
    }
#endif
    gptr buf = Crt_malloc(U32(A_FILE_LEFT)), p = buf;
    Pc_Read(fd, buf, U32(A_FILE_LEFT));
    Pc_Close(fd);
    if (U32(A_FILE_LEFT)) {
        do {
            gptr rec = U32(A_FILE_REC);
            if (U32(rec) <= 7) {
                g_file_handlers[U32(rec)](rec, p);
                rec = U32(A_FILE_REC);
            }
            u32 step = (U32(rec + 4) + 0x7FF) & ~0x7FFu;
            U32(A_FILE_REC) = rec + 0x20;
            p += step;
            U32(A_FILE_LEFT) -= step;
        } while (U32(A_FILE_LEFT));
    }
    Crt_free(buf);
    U16(A_FILE_STATE) = 0;
}

void DC2CALL File_Tick(void)                                        /* 0x403E40 */
{
    if (U16(A_FILE_STATE)) File_Process();
    else Rand8();
}

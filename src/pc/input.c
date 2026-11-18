/* The PC layer's key and display tables the options screen edits (game
 * memory only; reading the keyboard and switching the display stay in
 * pcio.c).
 *   0x865EFC  64 key bindings {u16 key (DirectInput code), u16 pad bit}
 *   0x73BDB8  their defaults
 *   0x73BB98  key names {u32 key, gptr name}, ended by key 0
 *   0x8660FC  10 joystick bindings {u16 button, u16 pad bit}
 *   0x75BE70  display modes per device and window/full screen (count, then
 *             {u16 w, h, bpp, pad}), 0x204 bytes each, 4 devices per kind
 *   0x75DEFC  devices, 0x164 bytes each: +0 can run in a window, +0x14 name */
#include "dc2/pcinput.h"

/* the name of key k (0 when it has none) */
gptr DC2CALL PcIn_KeyName(u32 k)                                     /* 0x4BF140 */
{
    u32 i = 0;
    if (U32(0x73BB98)) for (gptr e = 0x73BB98; U32(e) != k; ) { e += 8; i++; if (!U32(e)) break; }
    return U32(0x73BB98 + 8 * i) ? U32(0x73BB9C + 8 * i) : 0;
}

/* the key bound to pad bit b (0: none) */
u32 DC2CALL PcIn_KeyOf(u32 b)                                        /* 0x4BF1E0 */
{
    for (u32 i = 0; i < 64; i++)
        if (U16(0x865EFE + 4 * i) == b) return U16(0x865EFC + 4 * i);
    return 0;
}

/* pad bit b bound to key k */
void DC2CALL PcIn_BindKey(u32 b, u32 k)                              /* 0x4BF210 */
{
    for (u32 i = 0; i < 64; i++)
        if (U16(0x865EFE + 4 * i) == b) { U16(0x865EFC + 4 * i) = (u16)k; return; }
}

/* the default keys */
void DC2CALL PcIn_DefaultKeys(void)                                  /* 0x4BF250 */
{
    for (u32 i = 0; i < 0x100; i += 4) U32(0x865EFC + i) = U32(0x73BDB8 + i);
}

/* pad bit b bound to joystick button n */
void DC2CALL PcIn_BindButton(u32 b, u32 n)                           /* 0x4BF2A0 */
{
    for (u32 i = 0; i < 10; i++)
        if (U16(0x8660FE + 4 * i) == b) { U16(0x8660FC + 4 * i) = (u16)n; return; }
}

/* the index of mode w x h x bpp among those of the device and kind (0 when
   it is not there) */
static s32 mode_find(u32 dev, u32 win, u32 w, u32 h, u32 bpp)        /* 0x4BB9B0 */
{
    gptr l = 0x75BE70 + (dev + win * 4) * 0x204;
    for (s32 i = 0; i < S32(l); i++) {
        gptr m = l + 4 + 8 * i;
        if (U16(m) == w && U16(m + 2) == h && U16(m + 4) == bpp) return i;
    }
    return 0;
}

/* the next (dir 1), previous (-1) or same (0) display mode of the device:
   its size and depth, the device's name and whether it runs in a window
   (full screen when it cannot) */
void DC2CALL PcIn_DisplayStep(u32 dev, u32 win, gptr can_win, gptr name, gptr w, gptr h, gptr bpp, s32 dir) /* 0x4BB8D0 */
{
    gptr d = 0x75DEFC + dev * 0x164;
    if (!U32(d) && !win) win = 1;
    s32 i = mode_find(dev, win, U32(w), U32(h), U32(bpp));
    gptr l = 0x75BE70 + (dev + win * 4) * 0x204;
    if (dir == 1) { if (++i == S32(l)) i = 0; }
    else if (dir == -1) { if (--i < 0) i = S32(l) - 1; }
    gptr m = l + 4 + 8 * (u32)i;
    U32(w) = U16(m); U32(h) = U16(m + 2); U32(bpp) = U16(m + 4);
    U32(name) = d + 0x14;
    U32(can_win) = U32(d);
}

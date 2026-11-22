/* Unaligned data accesses on the SH-4 (Dreamcast build).
 *
 * The game's data was laid out for the x86, which reads a word or dword at
 * any address; the SH-4 raises an address error instead. The game code reads
 * memory with plain aligned loads (fast; the few unaligned places that are
 * known use UU16/UU32, include/dc2/types.h), and this handler catches the
 * rest: it decodes the faulting instruction (mov.w / mov.l / fmov.s, every
 * addressing mode GCC emits), does the access byte by byte and goes on with
 * the next instruction. Correct, but slow, so each place is counted and the
 * first ones are reported (DcUnaligned_Report: PCs to resolve with
 * sh-elf-addr2line -e dinocrisis2.elf). An access in a delayed branch's
 * slot (GCC puts the last store of a function in rts's) faults with the PC
 * at the branch: the slot's access is done and the branch taken here
 * (rts, bra, bsr, braf, bsrf, jmp, jsr, bt/s, bf/s). An instruction it
 * cannot handle (or an unaligned instruction fetch) goes to the handler
 * that was there before (KOS's crash report). */
#include <kos.h>
#include <stdio.h>
#include <arch/irq.h>
#include "dc2/types.h"
#include "unaligned.h"

#define TEA (*(volatile u32 *)0xFF00000C)     /* the faulting data address */

static irq_cb_t g_old_read, g_old_write;
static volatile u32 g_count;
#define NSITES 16
static volatile u32 g_site_pc[NSITES], g_site_n[NSITES];

static u32 ld(u32 a, int size)
{
    const u8 *p = (const u8 *)a;
    if (size == 2) return (u32)(s32)(s16)(p[0] | p[1] << 8);
    return (u32)p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}
static void st(u32 a, u32 v, int size)
{
    u8 *p = (u8 *)a;
    p[0] = (u8)v; p[1] = (u8)(v >> 8);
    if (size == 4) { p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
}

/* the visible single-precision register n (FPSCR.FR selects the bank) */
static u32 *fr(irq_context_t *c, int n) { return (c->fpscr & 0x200000) ? &c->frbank[n] : &c->fr[n]; }

/* emulate the data move op; 1 when done */
static int emulate(irq_context_t *c, u16 op)
{
    u32 *r = c->r;
    int n = (op >> 8) & 15, m = (op >> 4) & 15, d4 = op & 15;
    switch (op >> 12) {
    case 0x0:
        switch (op & 15) {
        case 0x5: st(r[0] + r[n], r[m], 2); return 1;                   /* mov.w Rm,@(R0,Rn) */
        case 0x6: st(r[0] + r[n], r[m], 4); return 1;                   /* mov.l Rm,@(R0,Rn) */
        case 0xD: r[n] = ld(r[0] + r[m], 2); return 1;                  /* mov.w @(R0,Rm),Rn */
        case 0xE: r[n] = ld(r[0] + r[m], 4); return 1;                  /* mov.l @(R0,Rm),Rn */
        }
        return 0;
    case 0x1: st(r[n] + d4 * 4, r[m], 4); return 1;                     /* mov.l Rm,@(disp,Rn) */
    case 0x2:
        switch (op & 15) {
        case 0x1: st(r[n], r[m], 2); return 1;                          /* mov.w Rm,@Rn */
        case 0x2: st(r[n], r[m], 4); return 1;                          /* mov.l Rm,@Rn */
        case 0x5: { u32 v = r[m]; r[n] -= 2; st(r[n], v, 2); return 1; }   /* mov.w Rm,@-Rn */
        case 0x6: { u32 v = r[m]; r[n] -= 4; st(r[n], v, 4); return 1; }   /* mov.l Rm,@-Rn */
        }
        return 0;
    case 0x5: r[n] = ld(r[m] + d4 * 4, 4); return 1;                    /* mov.l @(disp,Rm),Rn */
    case 0x6:
        switch (op & 15) {
        case 0x1: r[n] = ld(r[m], 2); return 1;                         /* mov.w @Rm,Rn */
        case 0x2: r[n] = ld(r[m], 4); return 1;                         /* mov.l @Rm,Rn */
        case 0x5: { u32 a = r[m]; if (n != m) r[m] += 2; r[n] = ld(a, 2); return 1; }  /* mov.w @Rm+,Rn */
        case 0x6: { u32 a = r[m]; if (n != m) r[m] += 4; r[n] = ld(a, 4); return 1; }  /* mov.l @Rm+,Rn */
        }
        return 0;
    case 0x8:
        if (n == 0x1) { st(r[m] + d4 * 2, r[0], 2); return 1; }        /* mov.w R0,@(disp,Rn) (Rn in bits 4-7) */
        if (n == 0x5) { r[0] = ld(r[m] + d4 * 2, 2); return 1; }       /* mov.w @(disp,Rm),R0 */
        return 0;
    case 0xC: {
        u32 d8 = op & 0xFF;
        switch (n) {
        case 0x1: st(c->gbr + d8 * 2, r[0], 2); return 1;               /* mov.w R0,@(disp,GBR) */
        case 0x2: st(c->gbr + d8 * 4, r[0], 4); return 1;               /* mov.l R0,@(disp,GBR) */
        case 0x5: r[0] = ld(c->gbr + d8 * 2, 2); return 1;              /* mov.w @(disp,GBR),R0 */
        case 0x6: r[0] = ld(c->gbr + d8 * 4, 4); return 1;              /* mov.l @(disp,GBR),R0 */
        }
        return 0;
    }
    case 0xF:
        if (c->fpscr & 0x100000) return 0;                              /* (SZ: pair moves, not handled) */
        switch (op & 15) {
        case 0x6: *fr(c, n) = ld(r[0] + r[m], 4); return 1;             /* fmov.s @(R0,Rm),FRn */
        case 0x7: st(r[0] + r[n], *fr(c, m), 4); return 1;              /* fmov.s FRm,@(R0,Rn) */
        case 0x8: *fr(c, n) = ld(r[m], 4); return 1;                    /* fmov.s @Rm,FRn */
        case 0x9: { u32 a = r[m]; r[m] += 4; *fr(c, n) = ld(a, 4); return 1; }   /* fmov.s @Rm+,FRn */
        case 0xA: st(r[n], *fr(c, m), 4); return 1;                     /* fmov.s FRm,@Rn */
        case 0xB: r[n] -= 4; st(r[n], *fr(c, m), 4); return 1;          /* fmov.s FRm,@-Rn */
        }
        return 0;
    }
    return 0;
}

/* the delayed branch at pc: 1 (and where it goes, and whether it links)
   when it is one; its target as the branch sees it, before its slot runs */
static int delayed_branch(irq_context_t *c, u16 op, u32 pc, u32 *to, int *link)
{
    u32 *r = c->r;
    int n = (op >> 8) & 15;
    s32 d12 = (s32)((op & 0xFFF) ^ 0x800) - 0x800, d8 = (s32)(s8)(op & 0xFF);
    *link = 0;
    if (op == 0x000B) { *to = c->pr; return 1; }                                /* rts */
    if ((op & 0xF000) == 0xA000) { *to = pc + 4 + d12 * 2; return 1; }          /* bra */
    if ((op & 0xF000) == 0xB000) { *to = pc + 4 + d12 * 2; *link = 1; return 1; } /* bsr */
    if ((op & 0xF0FF) == 0x0023) { *to = pc + 4 + r[n]; return 1; }             /* braf Rn */
    if ((op & 0xF0FF) == 0x0003) { *to = pc + 4 + r[n]; *link = 1; return 1; }  /* bsrf Rn */
    if ((op & 0xF0FF) == 0x402B) { *to = r[n]; return 1; }                      /* jmp @Rn */
    if ((op & 0xF0FF) == 0x400B) { *to = r[n]; *link = 1; return 1; }           /* jsr @Rn */
    if ((op & 0xFF00) == 0x8D00) { *to = (c->sr & 1) ? pc + 4 + d8 * 2 : pc + 4; return 1; }   /* bt/s */
    if ((op & 0xFF00) == 0x8F00) { *to = (c->sr & 1) ? pc + 4 : pc + 4 + d8 * 2; return 1; }   /* bf/s */
    return 0;
}

static void on_address_error(irq_t code, irq_context_t *c, void *data)
{
    (void)data;
    u32 tea = TEA;
    u32 to; int link;
    if (tea != c->pc && !(c->pc & 1) && delayed_branch(c, *(const u16 *)c->pc, c->pc, &to, &link)
        && emulate(c, *(const u16 *)(c->pc + 2))) {                    /* (the slot's access; then the branch) */
        u32 pc = c->pc;
        if (link) c->pr = pc + 4;
        c->pc = to;
        g_count++;
        for (int i = 0; i < NSITES; i++) {
            if (g_site_pc[i] == pc) { g_site_n[i]++; break; }
            if (!g_site_pc[i]) { g_site_pc[i] = pc; g_site_n[i] = 1; break; }
        }
        return;
    }
    if (tea != c->pc && !(c->pc & 1) && emulate(c, *(const u16 *)c->pc)) {
        u32 pc = c->pc;
        c->pc += 2;
        g_count++;
        for (int i = 0; i < NSITES; i++) {
            if (g_site_pc[i] == pc) { g_site_n[i]++; break; }
            if (!g_site_pc[i]) { g_site_pc[i] = pc; g_site_n[i] = 1; break; }
        }
        return;
    }
    irq_cb_t old = code == EXC_DATA_ADDRESS_WRITE ? g_old_write : g_old_read;
    if (old.hdl) { old.hdl(code, c, old.data); return; }
    printf("unaligned access at %08lX (PC %08lX, PR %08lX) not emulated\n",
           (unsigned long)tea, (unsigned long)c->pc, (unsigned long)c->pr);
    arch_abort();
}

void DcUnaligned_Init(void)
{
    g_old_read = irq_get_handler(EXC_DATA_ADDRESS_READ);
    g_old_write = irq_get_handler(EXC_DATA_ADDRESS_WRITE);
    irq_set_handler(EXC_DATA_ADDRESS_READ, on_address_error, NULL);
    irq_set_handler(EXC_DATA_ADDRESS_WRITE, on_address_error, NULL);
}

void DcUnaligned_Report(void)
{
    if (!g_count) return;
    printf("unaligned accesses emulated: %lu at", (unsigned long)g_count);
    for (int i = 0; i < NSITES && g_site_pc[i]; i++) printf(" %08lX(%lu)", (unsigned long)g_site_pc[i], (unsigned long)g_site_n[i]);
    printf("\n");
}

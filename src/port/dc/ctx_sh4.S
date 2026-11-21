! ctx_sh4.S - cooperative context switch for the task system on the SH-4
! (Dreamcast; KOS newlib has no ucontext). See ctx_sh4.h for the layout.
!
! GCC SH ABI: arguments in r4, r5; callee-saved r8-r14, r15 (sp), pr,
! fr12-fr15 and fpscr's mode bits. mach/macl are kept as well. C symbols carry
! a leading underscore on sh-elf.

    .text

! void dc2_ctx_swap(Dc2Ctx *save, const Dc2Ctx *load)
    .align  2
    .global _dc2_ctx_swap
_dc2_ctx_swap:
    mov.l   r8,  @(0,r4)
    mov.l   r9,  @(4,r4)
    mov.l   r10, @(8,r4)
    mov.l   r11, @(12,r4)
    mov.l   r12, @(16,r4)
    mov.l   r13, @(20,r4)
    mov.l   r14, @(24,r4)
    mov.l   r15, @(28,r4)
    sts     pr, r0
    mov.l   r0,  @(32,r4)
    sts     fpscr, r0
    mov.l   r0,  @(36,r4)
    sts     mach, r0
    mov.l   r0,  @(40,r4)
    sts     macl, r0
    mov.l   r0,  @(44,r4)
    ! fr12..fr15 (single moves: fpscr.SZ is 0 in the game's mode)
    mov     r4,  r1
    add     #48, r1
    fmov.s  fr12, @r1
    add     #4,  r1
    fmov.s  fr13, @r1
    add     #4,  r1
    fmov.s  fr14, @r1
    add     #4,  r1
    fmov.s  fr15, @r1

    mov.l   @(0,r5),  r8
    mov.l   @(4,r5),  r9
    mov.l   @(8,r5),  r10
    mov.l   @(12,r5), r11
    mov.l   @(16,r5), r12
    mov.l   @(20,r5), r13
    mov.l   @(24,r5), r14
    mov.l   @(28,r5), r15
    mov.l   @(32,r5), r0
    lds     r0,  pr
    mov.l   @(40,r5), r0
    lds     r0,  mach
    mov.l   @(44,r5), r0
    lds     r0,  macl
    mov     r5,  r1
    add     #48, r1
    fmov.s  @r1+, fr12
    fmov.s  @r1+, fr13
    fmov.s  @r1+, fr14
    fmov.s  @r1+, fr15
    ! fpscr last, after the moves made in the saved mode
    mov.l   @(36,r5), r0
    lds     r0,  fpscr
    rts
    nop

! u32 dc2_ctx_fpscr(void)
    .align  2
    .global _dc2_ctx_fpscr
_dc2_ctx_fpscr:
    sts     fpscr, r0
    rts
    nop

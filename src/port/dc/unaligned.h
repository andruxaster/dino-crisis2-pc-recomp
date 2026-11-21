/* Unaligned data accesses emulated in the address-error handler (unaligned.c). */
#ifndef DC2_UNALIGNED_H
#define DC2_UNALIGNED_H
void DcUnaligned_Init(void);
void DcUnaligned_Report(void);     /* how many so far, and where (PCs) */
#endif

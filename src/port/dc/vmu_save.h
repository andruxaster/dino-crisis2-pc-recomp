/* Saves on the VMU (vmu_save.c). */
#ifndef DC2_VMU_SAVE_H
#define DC2_VMU_SAVE_H
/* the saves' folder set to /ram, the VMU's saves unpacked into it, and each
   save the game writes from then on mirrored to the VMU */
void DcSave_Import(void);
void DcSave_Written(const char *host, const char *name);
#endif

/* /cd read by the port itself (dc_iso.c), in place of KallistiOS's fs_iso9660. */
#ifndef DC2_DC_ISO_H
#define DC2_DC_ISO_H
/* /cd taken over (before anything opens a file there); 0 done, -1 kept as it was */
int dc_iso_mount(void);
void dc_iso_report(void);              /* reads and folders so far (the log) */
extern volatile unsigned dc_iso_progress;   /* reads done (the watchdog: a slow load is not a hang) */
#endif

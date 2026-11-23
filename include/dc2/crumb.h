/* Breadcrumbs (the Dreamcast's DC2_DC_CRUMBS builds): a character written
 * straight to the log at points where the machine could stop, so that the
 * last ones in a log that ends show where it froze. g_crumb is NULL unless
 * a port sets it (defined in src/game/misc.c). */
#ifndef DC2_CRUMB_H
#define DC2_CRUMB_H
extern void (*g_crumb)(int c);
#define CRUMB(c) do { if (g_crumb) g_crumb(c); } while (0)
#endif

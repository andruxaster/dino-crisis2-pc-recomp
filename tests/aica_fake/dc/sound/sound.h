/* Stand-ins for KallistiOS (tests/aica_test.c): just what aica_sfx.c uses. */
#include <stdint.h>
#include <stddef.h>
uint32_t snd_mem_malloc(size_t size); void snd_mem_free(uint32_t addr); uint32_t snd_mem_available(void);
int snd_sh4_to_aica(void *packet, uint32_t size);

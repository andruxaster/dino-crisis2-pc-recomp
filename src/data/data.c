/* Data_Init: the tables of src/data copied to their original addresses in
 * the memory image (standalone builds). */
#include <string.h>
#include "dc2/data.h"

const DataBlock *const g_data_lists[] = { g_rodata, g_data_game, g_data_pc, 0 };

void Data_Init(void)
{
    for (int l = 0; g_data_lists[l]; l++)
        for (const DataBlock *b = g_data_lists[l]; b->size; b++) memcpy(MEM(b->addr), b->p, b->size);
}

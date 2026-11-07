/* Table of reimplemented functions: original address -> replacement.
 * The hook build writes a JMP at each address; the harness checks each
 * pair against the original code. Keep this list in address order. */
#include "dc2/game.h"
#include "hooks.h"

const Hook g_hooks[] = {
    {0x4041F0, (void *)Lzss_Decode,        "Lzss_Decode"},
    {0x46FAA0, (void *)Flag_Set,           "Flag_Set"},
    {0x46FB20, (void *)Flag_Check,         "Flag_Check"},
    {0x46FB50, (void *)Bits_Set,           "Bits_Set"},
    {0x46FB90, (void *)Bits_Check,         "Bits_Check"},
    {0x472210, (void *)Room_GlobalIndex,   "Room_GlobalIndex"},
    {0x4873C0, (void *)ScrOp_Load,         "ScrOp_Load"},
    {0x487400, (void *)ScrOp_Store,        "ScrOp_Store"},
    {0x487440, (void *)ScrOp_Arith,        "ScrOp_Arith"},
    {0x4874D0, (void *)ScrOp_Return,       "ScrOp_Return"},
    {0x487510, (void *)ScrOp_Push,         "ScrOp_Push"},
    {0x487550, (void *)ScrOp_Pop,          "ScrOp_Pop"},
    {0x487590, (void *)ScrOp_Branch,       "ScrOp_Branch"},
    {0x4875D0, (void *)ScrOp_Call,         "ScrOp_Call"},
    {0x487620, (void *)ScrOp_Wait,         "ScrOp_Wait"},
    {0x487B70, (void *)ScrOp_FlagSet,      "ScrOp_FlagSet"},
    {0x487BF0, (void *)ScrOp_FlagCheck,    "ScrOp_FlagCheck"},
    {0x489990, (void *)ScrOp_Nop,          "ScrOp_Nop"},
    {0x48B8E0, (void *)Script_GetValue,    "Script_GetValue"},
    {0x48B930, (void *)Script_SetValue,    "Script_SetValue"},
    {0x48B980, (void *)Script_BinOp,       "Script_BinOp"},
    {0x48BB20, (void *)Script_SetCond,     "Script_SetCond"},
    {0x48BC50, (void *)Script_Run,         "Script_Run"},
    {0x48BDB0, (void *)Script_StartThread, "Script_StartThread"},
    {0x48BE20, (void *)Script_KillThread,  "Script_KillThread"},
    {0x48F790, (void *)PointInQuad,        "PointInQuad"},
};
const int g_nhooks = sizeof g_hooks / sizeof g_hooks[0];

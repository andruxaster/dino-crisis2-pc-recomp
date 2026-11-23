# The GOG release's patches

The GOG Dino2.exe is the same build as the reference executable (link time
2002-02-05, identical layout) with about 40 small binary patches; the added
code lives in two code caves (0x4C0330..0x4C03D2, a replaced function, and
0x4E7048..0x4E7120, the padding at the end of .text). The 2006 Xplosiv / Red Ant re-release ships the reference executable
unchanged (same SHA-1). The Sourcenext release
is an earlier, different build (2002-01-28; .text 0xE63F8 bytes, .data
0x25000) with other addresses.

The harness keeps checking against the unpatched reference executable
(`g_gog_fixes` = 0). The native and Dreamcast ports set `g_gog_fixes` = 1 and
so carry the game fixes marked "ported" below; `DC2_ORIGINAL=1` or
`[Game] Original=1` in config.ini turns them off in the native port. The
saves go to a `saves` folder beside Data, as in the GOG release
(`DC2_SAVEDIR` still overrides; SAVExx.DAT files next to Data are moved in).

| Where | Patch | In the ports |
|-------|-------|--------------|
| 0x48C0B0 (Se_Play) | sound 0x38 (the tyrannosaur's roar) was never played; now played unless the source is 0x5F2C6C, 0x5ED1BC or (signed) below 0x200000 ("Restored missing T-Rex roar sound") | ported |
| 0x48C598 (Se_Start) | stereo option off: Snd_SetPan(h, 0x40) instead of keeping the last pan | ported |
| 0x48C718 (Snd_Tick) | stream volume applied every frame, not only while fading | ported |
| 0x404290 (File_Process) | file 0x78: fog off (PC_SetFogEnable(0), Light_SetFar(0)); file 0xEC: clears 0x746CE6 and 0x74B52E | ported |
| 0x4A5859 (title) | fog off before Flag_Set(3,0,0) / Movie_Start(0) | ported |
| 0x4A5ABF (title) | Title_Draw of the menu always, not only in state 4 | ported |
| 0x4A6018, 0x4A606A (Title_NewGame) | difficulty choice 0..2 (Easy available) instead of 1..2 | ported |
| 0x48C842 | sound volume `sar 3` -> `sar 4` (DirectSound attenuation) | PC only, not ported |
| 0x4BEDC4, Snd_SetVolume / Strm_SetVolume | dB range constant 3000/10000 -> 5000 (new float 0x704EAC), push 0xA -> 6 | PC only (DirectSound), not ported |
| 0x40C4A2 (AS_UpPanel) | x + 0x30 for the shop's item panel | not ported (unclear whether it is a renderer workaround) |
| 0x4B5F31 (PC_CalcVertexColour) | tpage 0x86, 0x95, 0x96 -> 0x40 under flag 0x73B67C | not ported (PC renderer) |
| Task_InitStackBase / Task_Start | task stacks 0x4000 -> 0x20000 | ports have their own stacks (fibers / ucontext 0x40000, Dreamcast 0x10000; measured use ~3.5 KB) |
| 0x401741, 0x46FD7A..0x470C71 (Win32 glue) | single instance (FindWindowA "DINO CRISIS 2"), SC_CLOSE handled | PC only |
| 0x477BE7, 0x477C02, 0x477C20, 0x477CD5, 0x47773A, 0x477771 | options display page: Menu_DrawFrames etc. calls removed | PC only |
| 0x4BFA42..0x4BFF48, 0x4C296B, 0x4C298F | small PC-layer changes, not yet examined | - |
| 0x4C0330 | replaced by `return 1` (the CD check) | not needed |
| .rdata 0x71A382 | movie interface string "dshow" | PC only |
| .data 0x71B221 | save path "saves\SAVE%02d.DAT" | done in the ports' file layer |
| .data 0x7375A8.. | movie table field 4 -> 2 | PC only |
| .data 0x737958.. | button presets | PC only |
| .data 0x73BC08..0x73BFFC | keyboard defaults and key names | PC only |

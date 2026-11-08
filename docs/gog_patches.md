# The GOG release's patches

The GOG Dino2.exe is the same build as the reference executable (link time
2002-02-05, identical layout) with about 40 small binary patches; the added
code lives in two code caves (0x4C0330..0x4C03D2, a replaced function, and
0x4E7048..0x4E7120, the padding at the end of .text). The 2006 Xplosiv / Red Ant re-release ships the reference executable
unchanged (same SHA-1). The Sourcenext release
is an earlier, different build (2002-01-28; .text 0xE63F8 bytes, .data
0x25000) with other addresses.

The harness keeps checking against the unpatched reference executable; the
list below says where GOG considered the original wrong or incompatible, for
the ports to decide case by case.

| Where | Patch |
|-------|-------|
| 0x401741 | string "DINO CRISIS 2" (window name for the check below) |
| 0x404290 (File_Process) | on file 0x78: fog off (PC_SetFogEnable(0), 0x48FB50(0)); on file 0xEC: clears 0x746CE6 and 0x74B52E |
| 0x40C4A2 | +0x30 after a CRT call (0x4DD89C) |
| 0x46FD7A..0x470C71 (Win32 glue) | FindWindowA check for the window (likely single instance); SC_CLOSE (0xF060) handled; other changes not yet examined |
| 0x477BE7, 0x477C02, 0x477C20, 0x477CD5 | 5-byte calls replaced (0x477BE7: call 0x495220 NOPed) |
| 0x47773A, 0x477771 | 1-byte changes, not yet examined |
| 0x48C0B0 (sound) | sound 0x38 played only for some sources (0x5F2C6C, 0x5ED1BC, addresses below 0x200000) |
| 0x48C598, 0x48C718, 0x48C844 | a sound parameter defaults to 0x40 when 0 |
| 0x4A5859 | fog off before 0x46FAA0 (Flag_Set) on this path |
| 0x4A5ABF, 0x4A6018, 0x4A606A | 1-2 byte changes, not yet examined |
| 0x4B5F31 | sound ids 0x86, 0x95, 0x96 mapped to 0x40 |
| 0x4BEDC4, 0x4BFA42..0x4BFF48 | 1-byte changes in the PC layer, not yet examined |
| 0x4C0330 | replaced by `return 1` (a check, probably for the CD) |
| 0x4C296B, 0x4C298F | 2-byte changes in the PC layer, not yet examined |
| .rdata 0x71A382 | movie interface string changed to "dshow" |
| .data 0x71B221 | save path "saves\SAVE%02d.DAT" |
| .data 0x7375C8..0x73795A | small table changes, not yet examined |
| .data 0x73BC08..0x73BFF0 | table changes (probably key mapping), not yet examined |

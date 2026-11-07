# dc2view

Minimal room viewer for Dino Crisis 2 PC data (Dino2.exe, 2002). Shows one
room camera: JPEG background, masks (16x16 background tiles with OT depth),
an animated skeletal model (the hero), the dinosaurs the room script places
(obj_spawn positions and the spawn points of the random spawner), an
optional floor grid and the room collision.

    make
    ./dc2view /path/to/Data ST100 --cam 0 --model WEP_P002 --anim 0

Options: `--cam N` (with `--pos` and no `--cam` the camera is chosen by the
room's camera zones), `--model FILE` (WEP_P*, E*, KOF_P*; default WEP_P002),
`--vram FILE` (extra VRAM uploads, e.g. `--vram E00.TEX` for Velociraptors; `dc2objects.py --json` prints the
`--model/--vram/--pos` for every enemy placed by a room script,
`--vram WEP_P002` for WEP_P000/WEP_P100 which carry no body texture),
`--anim N`, `--floor Y` (override the floor), `--collision`, `--no-follow`, `--pos X Z`, `--yaw A`, `--grid`, `--no-model`,
`--no-masks`, `--no-bg`, `--debug`, `--screenshot out.ppm --frames N`.

Keys: Left/Right camera (the model is placed under each camera's target
until you move it with WASD), C put the model back under the target,
Up/Down animation, Q/E turn, WASD move model: it stops at walls, takes the
floor level of the area it enters and switches camera by the room's camera
zones, as the game does (F turns this off), K show collision (yellow
walkable edge, red obstacle, green passable edge, magenta doors, cyan zones
of this camera),
N shows/hides the dinosaurs (`--no-enemies` at start; they load their
E*.DAT and, for Velociraptor/Oviraptor/Giganotosaurus, the E*.TEX of their
variant, like Enemy_LoadModel), Enter goes through the door the model stands in (doors are read from the
room script: target room, position, level, direction; `--door N` starts by
walking through door N), PgUp/PgDn floor height, G grid, M masks, B background, O model,
Space pause, Esc quit.

Verified: camera matrix equals the game's (Camera_LookAt + RotMatrixZXY);
projection follows the GTE emulation (sx = OFX + H*x/z, sy = OFY + H*y/z,
OFX/OFY = 160/120); drawn model pixels match the projected vertex bounds.

Known simplifications:
- floor height: the game stores levels, not heights (level = trunc(-y/1000));
  the viewer puts the floor at y = -1000*level, exact for flat floors, not
  for stairs/slopes (handled by scripts/animations in the game);
- every possible spawn place is shown at once (the game picks one random
  point per spawn, at most "max alive" of the active zone), all dinosaurs play
  their animation 0;
- doors ignore locks and the script conditions around them (a door defined
  behind a flag check is always listed);
- walls use the player's rules only (kind 0x80, edge bit 7 treated as wall);
- mask depth uses camera z = OT slot * 16; the game's exact conversion in
  Masks_SetQuad is not fully traced, so occlusion may be off;
- animation uses linear interpolation (the game: cubic over 4 keys),
  1 tick per frame at 60 Hz; no lighting (ncct), no fog, no semi-transparency;
- screen offset fixed at 160/120 (some scenes use other values).

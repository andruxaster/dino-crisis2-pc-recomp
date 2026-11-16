# dc2re -- Dino Crisis 2 PC recompilation
#   make            hook DLL (dinput.dll) + harness.exe (i686-w64-mingw32)
#   make check      run the harness under wine (DINO2=path/to/Dino2.exe DATA=path/to/Data)
#   make linux      game code built natively (64-bit): static library, smoke test, dc2 (the game, headless)
CC32   ?= i686-w64-mingw32-gcc
CC     ?= cc
CFLAGS  = -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude -fexcess-precision=standard -fno-strict-aliasing
GAME    = src/game/lzss.c src/game/flag.c src/game/geom.c src/game/script_vm.c \
          src/game/script_obj.c src/game/script_ops.c src/game/script_aot.c src/game/anim.c src/game/object.c src/game/player.c src/game/weapon.c src/game/zone.c src/game/gun.c src/game/door.c src/game/sound.c src/game/camera.c src/game/enemyset.c src/game/enemy.c src/game/raptor.c src/game/oviraptor.c src/game/allosaurus.c src/game/pteranodon.c src/game/trex.c src/game/plesio.c src/game/plesio2.c src/game/roamer.c src/game/gigano.c src/game/inostr.c src/game/tricera.c src/game/mosa.c src/game/compso.c src/game/kof.c src/game/kof_ino.c src/game/kof_tri.c src/game/kof_rap.c src/game/kof_ovi.c src/game/kof_allo.c src/game/kof_rex.c src/game/kof_comp.c src/game/aobj.c src/game/ptera2.c src/game/aobj2.c src/game/k2area.c src/game/effect.c src/game/k2.c src/game/misc.c src/game/draw.c src/pc/gpu.c src/pc/jpeg.c src/pc/gpudev.c src/pc/drawot.c src/pc/gpusoft.c src/game/crt.c src/game/file.c src/game/pcio.c src/game/room.c src/game/root.c src/game/title.c src/game/game.c src/game/task.c src/game/gte.c src/game/orig.c \
          src/hook/hooks.c
DINO2  ?= game/Dino2.exe
DATA   ?= game/Data
OUT     = build

all: $(OUT)/dinput.dll $(OUT)/harness.dll $(OUT)/harness_stub.exe

$(OUT):
	mkdir -p $(OUT)

HDRS    = $(wildcard include/dc2/*.h) src/hook/hooks.h

$(OUT)/dinput.dll: $(GAME) src/hook/dllmain.c $(HDRS) | $(OUT)
	$(CC32) $(CFLAGS) -DDC2_IN_PROCESS -shared -o $@ $(filter %.c,$^) -Wl,--kill-at -static-libgcc

$(OUT)/harness.dll: $(GAME) tests/harness.c $(HDRS) | $(OUT)
	$(CC32) $(CFLAGS) -DDC2_IN_PROCESS -shared -o $@ $(filter %.c,$^) -Wl,--image-base,0x10000000 -Wl,--out-implib,$(OUT)/libharness.a -static-libgcc

$(OUT)/harness_stub.exe: tests/stub.c $(OUT)/harness.dll
	$(CC32) -O2 -o $@ tests/stub.c -L$(OUT) -lharness -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -static-libgcc

WINEPREFIX ?= $(HOME)/.wine-dc2re
export WINEPREFIX
check: $(OUT)/harness_stub.exe
	@test -d $(WINEPREFIX) || WINEARCH=win32 wineboot -i >/dev/null 2>&1
	WINEDEBUG=-all wine $(OUT)/harness_stub.exe "$$(winepath -w $(DINO2))" "$$(winepath -w $(DATA))"

linux: $(OUT)/libdc2game.a $(OUT)/smoke $(OUT)/dc2
$(OUT)/libdc2game.a: $(GAME) $(HDRS) | $(OUT)
	for f in $(filter %.c,$^); do $(CC) $(CFLAGS) -c $$f -o $(OUT)/$$(basename $$f .c).o || exit 1; done
	ar rcs $@ $(addprefix $(OUT)/,$(notdir $(GAME:.c=.o)))
$(OUT)/smoke: src/port/smoke.c $(OUT)/libdc2game.a
	$(CC) $(CFLAGS) -o $@ $^ -lm
$(OUT)/dc2: src/port/dc2.c $(OUT)/libdc2game.a
	$(CC) $(CFLAGS) -o $@ $^ -lm
# the game in a window (SDL2, OpenGL)
dc2gl: $(OUT)/dc2gl
$(OUT)/dc2gl: src/port/dc2.c src/port/render_gl.c src/port/render_gl.h $(OUT)/libdc2game.a
	$(CC) $(CFLAGS) -DDC2_GL -o $@ $(filter %.c %.a,$^) $$(sdl2-config --cflags --libs) -lGL -lm

clean:
	rm -rf $(OUT)
.PHONY: all check linux dc2gl clean

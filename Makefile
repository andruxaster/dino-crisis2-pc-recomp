# dc2re -- Dino Crisis 2 PC recompilation
#   make            hook DLL (dinput.dll) + harness.exe (i686-w64-mingw32)
#   make check      run the harness under wine (DINO2=path/to/Dino2.exe DATA=path/to/Data)
#   make linux      game code built natively (64-bit) as a static library + smoke test
CC32   ?= i686-w64-mingw32-gcc
CC     ?= cc
CFLAGS  = -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude
GAME    = src/game/lzss.c src/game/flag.c src/game/geom.c src/game/script_vm.c
DINO2  ?= game/Dino2.exe
DATA   ?= game/Data
OUT     = build

all: $(OUT)/dinput.dll $(OUT)/harness.dll $(OUT)/harness_stub.exe

$(OUT):
	mkdir -p $(OUT)

$(OUT)/dinput.dll: $(GAME) src/hook/hooks.c src/hook/dllmain.c | $(OUT)
	$(CC32) $(CFLAGS) -DDC2_IN_PROCESS -shared -o $@ $^ -Wl,--kill-at -static-libgcc

$(OUT)/harness.dll: $(GAME) src/hook/hooks.c tests/harness.c | $(OUT)
	$(CC32) $(CFLAGS) -DDC2_IN_PROCESS -shared -o $@ $^ -Wl,--image-base,0x10000000 -Wl,--out-implib,$(OUT)/libharness.a -static-libgcc

$(OUT)/harness_stub.exe: tests/stub.c $(OUT)/harness.dll
	$(CC32) -O2 -o $@ tests/stub.c -L$(OUT) -lharness -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -static-libgcc

WINEPREFIX ?= $(HOME)/.wine-dc2re
export WINEPREFIX
check: $(OUT)/harness_stub.exe
	@test -d $(WINEPREFIX) || WINEARCH=win32 wineboot -i >/dev/null 2>&1
	WINEDEBUG=-all wine $(OUT)/harness_stub.exe "$$(winepath -w $(DINO2))" "$$(winepath -w $(DATA))"

linux: $(OUT)/libdc2game.a $(OUT)/smoke
$(OUT)/libdc2game.a: $(GAME) | $(OUT)
	for f in $(GAME); do $(CC) $(CFLAGS) -c $$f -o $(OUT)/$$(basename $$f .c).o || exit 1; done
	ar rcs $@ $(OUT)/lzss.o $(OUT)/flag.o $(OUT)/geom.o $(OUT)/script_vm.o
$(OUT)/smoke: src/port/smoke.c $(OUT)/libdc2game.a
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(OUT)
.PHONY: all check linux clean

# dc2view: needs SDL2 and OpenGL; JPEG via bundled stb_image.h
# Linux:   sudo apt install libsdl2-dev libgl-dev && make
# Windows: make win   (mingw-w64 cross compiler + SDL2-devel-*-mingw unpacked in SDL2_MINGW)
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
SDL2_MINGW ?= ../winbuild/SDL2-2.30.9/x86_64-w64-mingw32   # SDL2 MinGW dev package
WINCC ?= x86_64-w64-mingw32-gcc

dc2view: dc2view.c stb_image.h
	$(CC) $(CFLAGS) -o $@ dc2view.c $$(sdl2-config --cflags --libs) -lGL -lm

win: dc2view.exe
dc2view.exe: dc2view.c stb_image.h
	$(WINCC) $(CFLAGS) -o $@ dc2view.c -I$(SDL2_MINGW)/include/SDL2 -Dmain=SDL_main \
	  -L$(SDL2_MINGW)/lib -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lm -static-libgcc

clean:
	rm -f dc2view dc2view.exe

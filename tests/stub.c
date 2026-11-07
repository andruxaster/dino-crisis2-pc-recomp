/* harness_stub.exe: an image based at 0x400000 that spans the whole range of
 * Dino2.exe (big .bss), so the range is ours before anything else is
 * allocated. harness.dll then replaces the contents with the game image. */
#include <windows.h>
char dc2_reserve[0x500000];                     /* keeps the image >= 0x883000 */
__declspec(dllimport) void harness_main(int argc, char **argv);
int main(int argc, char **argv)
{
    ((volatile char *)dc2_reserve)[0] = 1;
    harness_main(argc, argv);                  /* never returns */
    return 0;
}

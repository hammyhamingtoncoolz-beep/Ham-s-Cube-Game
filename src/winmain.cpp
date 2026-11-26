#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main();

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif


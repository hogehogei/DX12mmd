#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
    va_list valist;
    va_start(valist, format);
    printf(format, valist);
    va_end(valist);
#endif
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
    DebugOutputFormatString("Show window test.");
    getchar();

    return 0;
}
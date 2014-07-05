#include <iostream>
#include <Windows.h>

void main()
{
    std::cout << "This is entry point" << std::endl;

    static int a = 0;
    while (true)
    {
        int b = a;
        std::cout << b << std::endl;
        ++a;
        Sleep(500);
    }
}
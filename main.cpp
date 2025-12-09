#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MessageBoxW(nullptr, L"test", L"test test", MB_OK);

	return 0;
}


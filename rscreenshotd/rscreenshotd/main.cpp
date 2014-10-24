#include <windows.h>
#include "server.h"

#define MAX_SERVER_FAILCNT 10

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	for (int i = 1; i < MAX_SERVER_FAILCNT; i++)
		server();
	return 1;
}

/*int main()
{
	for (int i = 1; i < MAX_SERVER_FAILCNT; i++)
		server();
	return 1;
}*/
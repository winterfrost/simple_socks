#include "std.h"
#include "Socks.h"

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
	WSADATA wdata;
	WSAStartup(0x0202,&wdata);
	Socks5 *s5 = new Socks5("127.0.0.1",4444);
	s5->Start();
	s5->Wait();
	return 0;
}

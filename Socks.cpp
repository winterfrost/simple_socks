#include "Socks.h"

Socks5::Socks5(const char *ip_addr, WORD port) : Thread()
{
	m_sock = INVALID_SOCKET;
	memset(&m_saddr,0,sizeof(sockaddr_in));
	m_saddr.sin_family = AF_INET;
	m_saddr.sin_addr.s_addr = inet_addr(ip_addr);
	m_saddr.sin_port = htons(port);
}

Socks5::~Socks5()
{
}

int Socks5::Run()
{
	m_sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (m_sock == INVALID_SOCKET)
		return 0;

	if (bind(m_sock,(sockaddr*)&m_saddr,sizeof(m_saddr)) == SOCKET_ERROR) {
		dbg("bind error %d",WSAGetLastError());
		Uninit();
		return 0;
	}
	if (listen(m_sock,15) == SOCKET_ERROR) {
		dbg("listen error %d",WSAGetLastError());
		Uninit();
		return 0;
	}
	SOCKET as = INVALID_SOCKET;
	while (1) {
		as = accept(m_sock,0,0);
		if (as == INVALID_SOCKET) {
			dbg("accept error %d",WSAGetLastError());
			Sleep(5);
			continue;
		}
		S5Conn *conn = new S5Conn(as);
		if (conn) {
			conn->Start();
		}
	}
}

void Socks5::Uninit()
{
	if (m_sock != INVALID_SOCKET) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
}

S5Conn::S5Conn( SOCKET s ) : Thread()
{
	m_sock = s;
	m_dst_sock = INVALID_SOCKET;
}

S5Conn::~S5Conn()
{
	dbg("closeing");
	if (m_sock != INVALID_SOCKET) 
		closesocket(m_sock);
	if (m_dst_sock != INVALID_SOCKET)
		closesocket(m_dst_sock);
}

int S5Conn::Run()
{
	int res = 0;
	char buf[1024];

	res = recv(m_sock,buf,1024,0);
	if (!res) {
		dbg("recv error %d",WSAGetLastError());
		return 0;
	}

	res = send(m_sock,"\x05\x00",2,0);
	if (res != 2) {
		dbg("send error %d",WSAGetLastError());
		return 0;
	}
	res = recv(m_sock,buf,4,0);
	if (res != 4) {
		dbg("recv error %d",WSAGetLastError());
		return 0;
	}
	S5Req req;
	if (!req.Parse(buf,4)) {
		dbg("wrong header");
		return 0;
	}
	S5Resp resp;

	if (req.ver != 5) {
		dbg("wrong version");
		resp.rep = 1;
	_err:
		send(m_sock,resp.Pack(),10,0);
		return 0;
	}
	if (req.cmd != 1) {
		dbg("wrong...");
		resp.rep = 7;
		goto _err;
	}

	switch (req.atyp) {
	case 1: // IPv4
		{
			res = recv(m_sock,buf,6,0);
			if (res != 6) {
				return 0;
			}
			if (!req.ParseIpv4(buf,res)) {
				return 0;
			}
		}
		break;
	
	case 3: // Domain name
		{
			char size = 0;
			res = recv(m_sock,&size,1,0);
			if (res != 1) {
				return 0;
			}
			res = recv(m_sock,buf,size+2,0);
			if (res != size+2) {
				return 0;
			}
			if (!req.ParseDomainName(buf,res+1)) {
				resp.rep = 4;
				goto _err;
			}
		}
		break;

	default:
		dbg("unknown address type");
		resp.rep = 8;
		goto _err;
	}
	
	sockaddr_in saddr = {0};
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = req.ipv4_addr;
	saddr.sin_port = req.port;
	
	m_dst_sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (connect(m_dst_sock,(sockaddr*)&saddr,sizeof(saddr)) == SOCKET_ERROR) {
		dbg("connect to destination error %d",WSAGetLastError());
		resp.rep = 4;
		goto _err;
	}
	dbg("connected...");
	resp.rep = 0;
	sockaddr_in dst_saddr = {0};
	int n = sizeof(sockaddr_in);

	if (!getsockname(m_dst_sock,(sockaddr*)&dst_saddr,&n)) {
		resp.bnd_addr = dst_saddr.sin_addr.s_addr;
		resp.bnd_port = dst_saddr.sin_port;
	}
	res = send(m_sock,resp.Pack(),10,0);
	if (res != 10) {
		dbg("send error %d",WSAGetLastError());
		return 0;
	}
	ForwardLoop();
	return 1;
}

int S5Conn::ForwardLoop()
{
	dbg("in");
	fd_set fds;
	int res;
	char buf[4096];

	timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 8;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(m_sock,&fds);
		FD_SET(m_dst_sock,&fds);

		res = select(0,&fds,0,0,&tv);
		if (res == SOCKET_ERROR)
			break;

		if (FD_ISSET(m_sock,&fds)) {
			res = recv(m_sock,buf,4096,0);
			if (res <= 0)
				break;
			res = send(m_dst_sock,buf,res,0);
		}

		if (FD_ISSET(m_dst_sock,&fds)) {
			res = recv(m_dst_sock,buf,4096,0);
			if (res <= 0)
				break;
			res = send(m_sock,buf,res,0);
		}
	_next:
		Sleep(3);
	}
	dbg("out");
	return 0;
}

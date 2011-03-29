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
	Uninit();
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
			if (!req.ParseDomainName(buf,res)) {
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
	fd_set r_set;
	fd_set w_set;
	int res;
	size_t buf_size = 4096;
	char *buf = (char *) malloc(buf_size);
	if (!buf) 
		return 0;

	timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 180;

	u_long mode = 1;
	ioctlsocket(m_sock,FIONBIO,&mode);
	ioctlsocket(m_dst_sock,FIONBIO,&mode);

	SocketForward *src = new SocketForward(m_sock);
	if (!src) 
		return 0;
	SocketForward *dst = new SocketForward(m_dst_sock,src);
	if (!dst) 
		return 0;
	src->SetForward(dst);

	tSocketForwardList fwlist;
	fwlist.push_back(src);
	fwlist.push_back(dst);
	tSocketForwardList::iterator iter;

	while(1) {
		FD_ZERO(&r_set);
		FD_ZERO(&w_set);

		for (iter=fwlist.begin();iter!=fwlist.end();++iter) {
			SocketForward *e = *iter;
			if (!e->status)
				FD_SET(e->sock,&r_set);
			FD_SET(e->sock,&w_set);
		}

		res = select(0,&r_set,&w_set,0,&tv);
		if (res == SOCKET_ERROR) {
			dbg("select error %d",WSAGetLastError());
			break;
		}
		if (!res) {
			dbg("timed out");
			break;
		}
		
		for (iter=fwlist.begin();iter!=fwlist.end();++iter) {
			SocketForward *e = *iter;
			
			if (FD_ISSET(e->sock,&r_set)) {
				u_long size = 0;
				res = ioctlsocket(e->sock,FIONREAD,&size);
				if (res == SOCKET_ERROR) {
					dbg("ioctlsocket error %d",WSAGetLastError());
					goto _end;
				}
				if (!size && !e->status) {
					e->status = 1;
					if (e->forward->buf.empty())
						goto _end;
					closesocket(e->sock);
				} else {
					if (size > buf_size) {
						buf_size = size;
						free(buf);
						buf = (char*)malloc(buf_size);
						if (!buf) 
							goto _end;
					}
					res = recv(e->sock,buf,s_buf_size,0);
					if (res <= 0) {
						dbg("recv error %d",WSAGetLastError());
						goto _end;
					}
					SocketForward *to = e->forward;
					to->ForwardBuf(buf,res);
				}
			}
			
			if (FD_ISSET(e->sock,&w_set)) {
				if (e->status == 2 || e->forward->status == 1) {
					dbg("end");
					goto _end;
				}
				size_t size = e->buf.size();
				res = send(e->sock,e->buf.c_str(),size,0);
				e->buf.clear();
				if (res == SOCKET_ERROR) {
					dbg("send error %d, %d bytes",WSAGetLastError(),size);
					goto _end;
				}
				if (e->status) 
					e->status = 2;
			}
		}
		Sleep(1);
	}

_end:
	if (buf) 
		free(buf);
	delete src;
	delete dst;
	dbg("out");
	return 0;
}

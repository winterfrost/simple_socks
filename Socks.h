#pragma once
#include "std.h"
#include <list>
#include "Thread.h"

struct S5Req 
{
	BYTE ver, cmd, rsv, atyp;
	DWORD ipv4_addr;
	WORD port;
	char *domain_name;

	S5Req() {
		ver = cmd = rsv = atyp = 0;
		ipv4_addr = 0;
		port = 0;
		domain_name = 0;
	}
	~S5Req()
	{
		if (domain_name) 
			free(domain_name);
	}
	int Parse(char *buf, int size) {
		if (size < 4)
			return 0;
		ver = *buf++;
		cmd = *buf++;
		rsv = *buf++;
		atyp = *buf++;
		return 1;
	}
	int ParseIpv4(char *buf, int size) {
		if (size < sizeof(DWORD) + sizeof(WORD))
			return 0;
		ipv4_addr = *(DWORD*)buf;
		port = *(WORD*)(buf+4);
		return 1;
	}

	int ParseDomainName(char *buf, int size) {
		if (size < 3) 
			return 0;
		int name_size = size - 2;
		domain_name = (char*)malloc(name_size + 1);
		memset(domain_name,0,name_size + 1);
		memcpy(domain_name,buf,name_size);
		port = *(WORD*)(buf+name_size);

		struct hostent *host = gethostbyname(domain_name);
		if (!host || host->h_addrtype != AF_INET || !host->h_addr)
			return 0;
		ipv4_addr = *(DWORD*)host->h_addr;
		if (!ipv4_addr)
			return 0;
		return 1;
	}
};

struct S5Resp 
{
	BYTE ver, rep, rsv, atyp;
	DWORD bnd_addr;
	WORD bnd_port;
	char packet[10];
	
	S5Resp() {
		ver = 5;
		rep = 1;
		rsv = 0;
		atyp = 1;
		bnd_addr = 0;
		bnd_port = 0;
	}
	~S5Resp() {

	}

	char *Pack() {
		memset(packet,0,10);
		packet[0] = ver;
		packet[1] = rep;
		packet[2] = rsv;
		packet[3] = atyp;
		*(DWORD*)&packet[4] = bnd_addr;
		*(WORD*)&packet[8] = bnd_port;
		return packet;
	}
};

class S5Conn : public Thread
{
	struct SocketForward;
	typedef std::list<SocketForward*> tSocketForwardList;
public:
	S5Conn(SOCKET s);
	virtual ~S5Conn();
	int Run();
private:
	SOCKET m_sock;
	SOCKET m_dst_sock;

	int ForwardLoop();
};


struct S5Conn::SocketForward 
{
	SOCKET sock;
	SocketForward *forward;
	std::string buf;
	int status;

	SocketForward(SOCKET s1,SocketForward *fw = 0) {
		sock = s1;
		forward = fw;
		status = 0;
	}
	~SocketForward() {
		buf.clear();
	}
	void SetForward(SocketForward* fw) {
		forward = fw;
	}
	void ForwardBuf(char *data, size_t size) {
		buf.append(data,size);
	}
};


class Socks5 : public Thread
{
public:
	Socks5(const char *ip_addr, WORD port);
	virtual ~Socks5();
	int Run();
	void Uninit();
private:
	SOCKET m_sock;
	sockaddr_in m_saddr;
};



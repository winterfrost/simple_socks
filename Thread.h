#pragma once
#include "std.h"

class Thread 
{
public:
	HANDLE m_thread;
	DWORD m_id;
	Thread();
	virtual ~Thread();
	virtual int Run() = 0;
	int Start();
	void Terminate(DWORD exit_code = 0);
	int Wait();
	
	operator HANDLE() {
		return m_thread;
	}
private:
	static DWORD WINAPI Worker(PVOID param);
};

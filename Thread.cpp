#include "Thread.h"

Thread::Thread()
{
	m_thread = 0;
}

Thread::~Thread()
{
	if (m_thread) 
		CloseHandle(m_thread);
}

int Thread::Start()
{
	m_thread = CreateThread(0,0,Worker,this,0,0);
	return m_thread ? 1:0;
}

void Thread::Terminate()
{
	if (m_thread) {
		TerminateThread(m_thread,0);
		CloseHandle(m_thread);
		m_thread = 0;
	}
}

int Thread::Wait()
{
	if (WaitForSingleObject(m_thread,INFINITE) == WAIT_OBJECT_0) 
		return 1;
	return 0;
}

DWORD WINAPI Thread::Worker(PVOID param)
{
	Thread* self = (Thread*)param;
	DWORD ret = self->Run();
	CloseHandle(self->m_thread);
	self->m_thread = 0;
	delete self;
	return ret;
}

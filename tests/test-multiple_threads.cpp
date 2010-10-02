#include <stdio.h>
#include <Windows.h>

#include "insight/redist/insight.h"

DWORD WINAPI threadproc(void*)
{
	for(int i = 0; i<1000000; ++i)
	{
		Insight::Scope scope_timing("Main loop (sleep 50ms)");

		for(int i=0; i<10; ++i)
		{
			Insight::Scope scope_timing("Inner loop (sleep 20ms)");
			Sleep(20);
		}
		{
			Insight::Scope scope_timing("Sleep 50ms");
			Sleep(50);
		}
	}

	return 0;
}

int main()
{
	Insight::initialize(true);

	static const int NUM_THREADS = 8;

	HANDLE thread_handles[NUM_THREADS];

	for( int i=0; i<NUM_THREADS; ++i )
	{
		thread_handles[i] = CreateThread(0, 0, threadproc, 0, 0, 0);
	}

	WaitForMultipleObjects(NUM_THREADS, thread_handles, TRUE, INFINITE);

	Insight::terminate();

	return 0;
}



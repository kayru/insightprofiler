#include <stdio.h>
#include <Windows.h>

#include "insight/redist/insight.h"

int main()
{
	Insight::initialize(true);

	for(int i = 0; i<1000000; ++i)
	{
		Insight::Scope scope_timing("Main loop (sleep 50ms)");

		for(int i=0; i<10; ++i)
		{
			Insight::Scope scope_timing("Inner loop (sleep 20ms)");
			Sleep(20);
		}
		Sleep(50);
	}

	Insight::terminate();

	return 0;
}



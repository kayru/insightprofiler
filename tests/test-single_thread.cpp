#include <stdio.h>
#include <Windows.h>

#include "insight/redist/insight.h"

int main()
{
	Insight::initialize(false);

	for(int i = 0; i<1000000; ++i)
	{
		Insight::Scope scope_timing("Main loop (sleep 50ms)");

		for(int i=0; i<10; ++i)
		{
			Insight::Scope scope_timing("Inner loop (sleep 20ms)");
			Sleep(20);

			Insight::update();
		}
		Sleep(50);

		Insight::update();
	}

	Insight::terminate();

	return 0;
}



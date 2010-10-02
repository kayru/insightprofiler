#include <stdio.h>
#include <Windows.h>

#include "insight/redist/insight.h"

int main()
{
	Insight::initialize(false);

	for(int i = 0; i<1000000; ++i)
	{
		Insight::Scope scope_timing("Main loop");

		for(int i=0; i<10; ++i)
		{
			Insight::Scope scope_timing("Inner loop");
			{
				Insight::Scope scope_timing("Sleep 1ms");
				Sleep(1);
			}
		}

		{
			Insight::Scope scope_timing("Sleep 2ms");
			Sleep(2);
		}

		Insight::update();
	}

	Insight::terminate();

	return 0;
}



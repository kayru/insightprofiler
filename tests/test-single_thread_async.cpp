#include <stdio.h>
#include <Windows.h>

#define USE_INSIGHT_PROFILER

#ifdef USE_INSIGHT_PROFILER
#include "insight/redist/insight.h"
#pragma comment(lib, "insight.lib")
#define INSIGHT_INITIALIZE	Insight::initialize();
#define INSIGHT_TERMINATE	Insight::terminate();
#define INSIGHT_SCOPE(name) Insight::Scope __insight_scope(name);
#else
#define INSIGHT_INITIALIZE
#define INSIGHT_TERMINATE
#define INSIGHT_SCOPE(name)
#endif //RUSH_INSIGHT

int main()
{
	INSIGHT_INITIALIZE;

	for(int i = 0; i<1000000; ++i)
	{
		INSIGHT_SCOPE("Main loop (sleep 50ms)");

		for(int i=0; i<10; ++i)
		{
			INSIGHT_SCOPE("Inner loop (sleep 20ms)");
			Sleep(20);
		}
		Sleep(50);
	}

	INSIGHT_TERMINATE;

	return 0;
}



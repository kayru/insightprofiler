#include "insight/redist/insight.h"

#include "insight_utils.h"
#include "insight_gui.h"
#include "insight_backend.h"

#define NOMINMAX
#include <Windows.h> // for GetCurrentThreadId()
#include <algorithm>


namespace
{
	bool g_asynchronous = false;
}

namespace Insight
{

	//////////////////////////////////////////////////////////////////////////

	void initialize(bool asynchronous)
	{
		g_asynchronous = asynchronous;

		InsightBackend::g_token_dummy.name = "!!! DUMMY INSIGHT TOKEN !!!";
		InsightGui::initialize(asynchronous);
	}

	void terminate()
	{
		InsightGui::terminate();
	}

	void update()
	{
		InsightGui::update();
	}

	Token* enter(const char* name)
	{
		Token* e = InsightBackend::g_token_pool.alloc();

		if( e )
		{
			e->name = name;
			e->time_enter = __rdtsc();
			e->thread_id = GetCurrentThreadId();
		}
		else
		{
			e = &InsightBackend::g_token_dummy;
		}

		return e;
	}

	void exit( Token* e )
	{
		if( e != &InsightBackend::g_token_dummy )
		{
			e->time_exit = __rdtsc();
			InsightBackend::g_token_buffer.push(*e);
			InsightBackend::g_token_pool.free(e);
		}
	}

}


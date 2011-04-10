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

	void initialize(bool asynchronous, bool start_minimized)
	{
		g_asynchronous = asynchronous;
		InsightGui::initialize(asynchronous, start_minimized);
	}

	void terminate()
	{
		InsightGui::terminate();
	}

	void update()
	{
		InsightGui::update();
	}

	Token enter(const char* name)
	{
		Token token;

		token.time_enter = __rdtsc();
		token.thread_id = GetCurrentThreadId();
		token.name = name;

		return token;
	}

	void exit( Token& token )
	{
		token.time_exit = __rdtsc();
		InsightBackend::g_token_buffer.push(token);
	}

}


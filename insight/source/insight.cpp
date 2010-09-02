#include "insight/redist/insight.h"

#include "insight_utils.h"
#include "insight_gui.h"

#define NOMINMAX
#include <Windows.h> // for GetCurrentThreadId()
#include <algorithm>

namespace
{
	const size_t MAX_PROFILE_TOKENS =	16384;
	typedef InsightUtils::Pool<Insight::Token, MAX_PROFILE_TOKENS> TokenPool;
	typedef InsightUtils::Buffer<Insight::Token, MAX_PROFILE_TOKENS> TokenBuffer;

	Insight::Token	g_token_dummy;
	TokenPool		g_token_pool;
	TokenBuffer		g_token_buffer;
	TokenBuffer		g_token_back_buffer;

	void snapshot()
	{
		long num_tokens = std::min(long(MAX_PROFILE_TOKENS), g_token_buffer.lock());
		memcpy(&g_token_back_buffer, &g_token_buffer, sizeof(g_token_buffer));
		g_token_buffer.flush();
		g_token_back_buffer.pos.set(num_tokens);
	}
}

namespace Insight
{

	//////////////////////////////////////////////////////////////////////////

	void initialize()
	{
		g_token_dummy.name = "!!! DUMMY INSIGHT TOKEN !!!";
		InsightGui::initialize();
	}

	void terminate()
	{
		InsightGui::terminate();
	}

	Token* enter(const char* name)
	{
		Token* e = g_token_pool.alloc();

		if( e )
		{
			e->name = name;
			e->time_enter = __rdtsc();
			e->thread_id = GetCurrentThreadId();
		}
		else
		{
			e = &g_token_dummy;
		}

		return e;
	}

	void exit( Token* e )
	{
		if( e != &g_token_dummy )
		{
			e->time_exit = __rdtsc();
			g_token_pool.free(e);
		}
	}

}


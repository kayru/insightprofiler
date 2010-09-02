#include "insight_backend.h"
#include "insight_utils.h"
#include <algorithm>

namespace InsightBackend
{
	Insight::Token	g_token_dummy;
	TokenPool		g_token_pool;
	TokenBuffer		g_token_buffer;
	TokenBuffer		g_token_back_buffer;

	//////////////////////////////////////////////////////////////////////////

	void snapshot()
	{
		long lock_count = g_token_buffer.lock();
		long num_tokens = std::min(long(MAX_PROFILE_TOKENS), lock_count);
		memcpy(&g_token_back_buffer, &g_token_buffer, sizeof(g_token_buffer));
		g_token_buffer.flush();
		g_token_back_buffer.pos.set(num_tokens);
	}


}


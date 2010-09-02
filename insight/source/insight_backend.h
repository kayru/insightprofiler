#ifndef __INSIGHT_BACKEND_H__
#define __INSIGHT_BACKEND_H__

#include "insight/redist/insight.h"
#include "insight_utils.h"

namespace InsightBackend
{
	const size_t MAX_PROFILE_TOKENS =	16384;
	typedef InsightUtils::Pool<Insight::Token, MAX_PROFILE_TOKENS> TokenPool;
	typedef InsightUtils::Buffer<Insight::Token, MAX_PROFILE_TOKENS> TokenBuffer;

	extern Insight::Token	g_token_dummy;
	extern TokenPool		g_token_pool;
	extern TokenBuffer		g_token_buffer;
	extern TokenBuffer		g_token_back_buffer;

	extern void snapshot();

}

#endif // __INSIGHT_BACKEND_H__


#ifndef __INSIGHT_BACKEND_H__
#define __INSIGHT_BACKEND_H__

#include "insight/redist/insight.h"
#include "insight_utils.h"

namespace InsightBackend
{
	const size_t MAX_PROFILE_TOKENS =	16384;
	typedef InsightUtils::Buffer<Insight::Token, MAX_PROFILE_TOKENS> TokenBuffer;

	extern TokenBuffer		g_token_buffer;
	extern TokenBuffer		g_token_back_buffer;

	extern void snapshot();
}

#endif // __INSIGHT_BACKEND_H__


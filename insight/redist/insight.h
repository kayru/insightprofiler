#ifndef __INSIGHT_H__
#define __INSIGHT_H__

#ifdef INSIGHT_DLL
	#define INSIGHT_API __declspec( dllexport )
#else
	#define INSIGHT_API __declspec( dllimport )
#endif //INSIGHT_DLL

// profiler helper macros

#ifdef INSIGHT_ENABLE
	#define INSIGHT_INIT Insight::Initializer __insight_initializer;
	#define INSIGHT_SCOPE(name) Insight::Scope __insight_scope(name);
#else 
	#define INSIGHT_INIT
	#define INSIGHT_SCOPE(name)
#endif //INSIGHT_ENABLE

namespace Insight
{
	typedef unsigned __int64 cycle_metric;

	struct Token
	{
		unsigned __int64	time_enter;
		unsigned __int64	time_exit;
		unsigned long		thread_id;
		const char*			name;
	};

	extern INSIGHT_API void initialize();								// call this at application startup
	extern INSIGHT_API void terminate();								// call this at application shutdown

	extern INSIGHT_API Token* enter(const char* name);					// start profile event (enter scope)
	extern INSIGHT_API void   exit(Token* token);						// finish profile event (exit scope)

	class Node
	{
	public:
		Node(const char* name)
			: m_token(0)
			, m_name(name)
		{
		}
		__forceinline void start()
		{
			m_token = Insight::enter(m_name);
		}
		__forceinline void stop()
		{
			Insight::exit(m_token);
			m_token = 0;
		}
		__forceinline const char* name() const { return m_name; }
	private:
		Token*		m_token;
		const char* m_name;
	};

	class Scope
	{
	public:
		explicit __forceinline Scope(const char* name)
			: m_token(0)
		{
			m_token = Insight::enter(name);
		}
		explicit __forceinline Scope(const Node& node)
			: m_token(0)
		{
			m_token = Insight::enter(node.name());
		}
		__forceinline ~Scope()
		{
			Insight::exit(m_token);
		}
	private:
		Token*	m_token;
	};
	
}

#endif //__INSIGHT_H__


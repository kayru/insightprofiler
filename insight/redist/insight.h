#ifndef __INSIGHT_H__
#define __INSIGHT_H__

#ifdef INSIGHT_DLL
	#define INSIGHT_API __declspec( dllexport )
#else
	#define INSIGHT_API __declspec( dllimport )
#endif //INSIGHT_DLL

namespace Insight
{
	typedef unsigned __int64 cycle_metric;

	struct Token
	{
		cycle_metric	time_enter;
		cycle_metric	time_exit;
		unsigned long	thread_id;
		const char*		name;
	};

	INSIGHT_API void initialize(bool asynchronous=true, bool start_minimized=false);	// call this at application startup
	INSIGHT_API void terminate();														// call this at application shutdown

	INSIGHT_API void update();															// call this once a frame if using synchronous mode

	INSIGHT_API Token enter(const char* name);											// start profile event (enter scope); 'name' must be static
	INSIGHT_API void  exit(Token& token);												// finish profile event (exit scope)

	class Node
	{
	public:
		Node(const char* name)
			: m_name(name)
		{
		}
		__forceinline void start()
		{
			m_token = Insight::enter(m_name);
		}
		__forceinline void stop()
		{
			Insight::exit(m_token);
		}
		__forceinline const char* name() const { return m_name; }
	private:
		Token m_token;
		const char* m_name;
	};

	class Scope
	{
	public:
		explicit __forceinline Scope(const char* name)
		{
			m_token = Insight::enter(name);
		}
		explicit __forceinline Scope(const Node& node)
		{
			m_token = Insight::enter(node.name());
		}
		__forceinline ~Scope()
		{
			Insight::exit(m_token);
		}
	private:
		Token	m_token;
	};

}

#endif //__INSIGHT_H__


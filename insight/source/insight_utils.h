#ifndef __INSIGHT_UTILS_H__
#define __INSIGHT_UTILS_H__

#include <intrin.h>

namespace InsightUtils
{
	struct AtomicInt
	{	
		inline AtomicInt() : val(0)
		{}

		// returns current value
		inline long get()
		{
			return _InterlockedOr(&val, 0);
		}

		// sets value to x and returns old value
		inline long set(long x)
		{
			return _InterlockedExchange(&val, x);
		}

		// compare and swap
		inline long cas(long x, long c)
		{
			return _InterlockedCompareExchange(&val, x, c);
		}

		// increments value and returns old value
		inline long inc()
		{
			return _InterlockedIncrement(&val)-1;
		}

		// decrements value and returns old value
		inline long dec()
		{
			return _InterlockedDecrement(&val)+1;
		}

		volatile long val;
	};

	//////////////////////////////////////////////////////////////////////////

	template <typename T, size_t SIZE>
	class Buffer
	{
	public:

		inline Buffer()
		{
			pos.set(0);
		}

		inline bool push(const T& v)
		{
			long idx = pos.inc();
			if( idx < SIZE )
			{
				data[idx] = v;
				return true;
			}
			else
			{
				pos.dec();
				return false;
			}
		}

		inline long lock_and_get_size()
		{
			return pos.set(SIZE+1);
		}
		inline void flush()
		{
			pos.set(0);
		}

		T			data[SIZE];
		AtomicInt	pos;

	};

	//////////////////////////////////////////////////////////////////////////

	template <typename T, size_t SIZE>
	struct Stack
	{
		inline Stack() : pos(0) { }

		inline bool push(const T& v)
		{
			if( pos+1 < SIZE )
			{
				data[pos] = v;
				++pos;
				return true;
			}
			else
			{
				return false;
			}
		}
		inline bool pop(T& v)
		{
			if( pos > 0 )
			{
				v = data[pos];
				--pos;
				return true;
			}
			else
			{
				return false;
			}
		}
		inline bool pop()
		{
			if( pos > 0 )
			{
				--pos;
				return true;
			}
			else
			{
				return false;
			}
		}
		
		inline const T& peek() const { return data[pos-1]; }
		inline size_t size() const { return pos;	}
		inline size_t max_size() const { return SIZE; }
		inline void reset() { pos = 0; }

		inline bool ensure_size( size_t to_size )
		{
			if( pos+1 < to_size )
			{
				return true;
			}
			else if( to_size <= SIZE )
			{
				pos = to_size;
				return true;
			}
			else 
			{
				return false;
			}
		}

		inline const T&	operator [] (size_t idx) const	{ return data[idx];	}
		inline T&		operator [] (size_t idx)		{ return data[idx];	}

		T		data[SIZE];
		size_t	pos;
	};

	//////////////////////////////////////////////////////////////////////////

}
#endif // __INSIGHT_UTILS_H__


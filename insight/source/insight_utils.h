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

	struct Spinlock
	{
		inline Spinlock()
		{
			lock.set(0);
		}

		inline void enter()
		{
			while(lock.cas(1,0)==1)
			{
			}
		}
		inline void exit()
		{
			lock.set(0);
		}

		AtomicInt lock;
	};

	//////////////////////////////////////////////////////////////////////////

	template <typename T, size_t SIZE>
	class Pool
	{

	public:

		Pool()
		{
			pos = 0;
			for( size_t i=0; i<SIZE; ++i )
			{
				freelist[i] = &data[i];
			}
		}

		~Pool()
		{
		}

		inline T* alloc()
		{	
			lock.enter();
			T* res = NULL;
			if( pos < SIZE )
			{
				res = freelist[pos];
				++pos;
			}
			lock.exit();
			return res;
		}

		inline void free(T* ptr) 
		{
			lock.enter();
			--pos;
			freelist[pos] = ptr;
			lock.exit();
		}

	private:

		T			data[SIZE];
		T*			freelist[SIZE];
		size_t		pos;
		Spinlock	lock;

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

		inline long lock()
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
		inline Stack()	: pos(0) { }

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

	// Random number generator as described here: http://support.microsoft.com/kb/28150
	class Rand
	{
	public:

		inline Rand(unsigned long seed) 
			: m_seed(seed)
		{}

		inline unsigned long operator()()
		{
			m_seed=214013*m_seed+2531011;
			return (m_seed ^ m_seed>>16);
		}

		inline float get_float(float min, float max)
		{
			m_seed=214013*m_seed+2531011;
			return min+(m_seed>>16)*(1.0f/65535.0f)*(max-min);
		}

	private:

		unsigned long m_seed;

	};

}
#endif // __INSIGHT_UTILS_H__


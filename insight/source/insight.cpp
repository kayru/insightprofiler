#define NOMINMAX
#include <stdio.h>
#include <Windows.h>
#include <algorithm>
#include <vector>

#include "insight/redist/insight.h"

#include "insight_utils.h"
#include "insight_gui.h"

#pragma warning(disable:4101)
#pragma warning(disable:4189)

#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif //GET_WHEEL_DELTA_WPARAM

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif //WM_MOUSEWHEEL

namespace Insight
{		
	const size_t MAX_PROFILE_TOKENS =	16384;
	const size_t MAX_REPORT_TEXT	=	16384;
	
	//////////////////////////////////////////////////////////////////////////

	typedef Pool<Token, MAX_PROFILE_TOKENS>		TokenPool;
	typedef Buffer<Token, MAX_PROFILE_TOKENS>	TokenBuffer;
		
	//////////////////////////////////////////////////////////////////////////

	class Manager;
		
	HANDLE	g_thread = 0;
	
	cycle_metric_t g_cycles_per_ms = 1;

	Rand g_rand(1);
	
	//////////////////////////////////////////////////////////////////////////

	static LRESULT	APIENTRY	wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static DWORD	WINAPI		threadproc(void* param);
			
	//////////////////////////////////////////////////////////////////////////
	
	__forceinline cycle_metric_t cycle_count()
	{
		return __rdtsc();
	}

	float cycles_to_ms(cycle_metric_t cycles)
	{
		float res = float (double(cycles) / double(g_cycles_per_ms));
		return res;
	}

	//////////////////////////////////////////////////////////////////////////

	struct TokenSorter
	{
		bool operator()(const Token& a, const Token& b)
		{
			if( a.thread_id == b.thread_id )
			{
				return a.time_start < b.time_start;
			}
			else
			{
				return a.thread_id < b.thread_id;
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	
	template <size_t SIZE>
	class Text
	{
	public:
		Text()
			: m_pos(0)
		{
			reset();
		}
		void reset()
		{
			m_pos = 0;
			memset(m_buffer, 0, SIZE);
		}

		void print(const char* str, ...)
		{
			char buf[1024];

			va_list ap;
			va_start(ap, str);
			int len = vsprintf_s(buf, str, ap);
			va_end(ap);

			if(m_pos + len < MAX_REPORT_TEXT)
			{
				memcpy(m_buffer+m_pos, buf, len);
				m_pos += len;
			}
		}
		void print_padded(int pad, char ch, const char* str, ...)
		{	
			char buf[1024];

			va_list ap;			
			va_start(ap, str);
			int len = vsprintf_s(buf, str, ap);
			va_end(ap);

			if( len < pad)
			{
				memset(buf+len, ch, pad-len);
			}
			len = pad;

			if(m_pos + len < MAX_REPORT_TEXT)
			{
				memcpy(m_buffer+m_pos, buf, len);
				m_pos += len;
			}
		}

		const char* buffer() const { return m_buffer; }
		const size_t length() const { return m_pos; }

		void draw(HDC hdc, RECT rt)
		{
			FillRect(hdc, &rt, (HBRUSH) (COLOR_WINDOW+1));

			HGDIOBJ  hfnt, hfnt_old;
			hfnt = GetStockObject(ANSI_FIXED_FONT);
			hfnt_old = SelectObject(hdc, hfnt);
			DrawText(hdc, buffer(), length(), &rt, DT_LEFT|DT_EXPANDTABS);
			SelectObject(hdc, hfnt_old);
		}

	private:
		char	m_buffer[SIZE];
		size_t	m_pos;
	};

	class Manager
	{
	public:
		Manager()
		{
			m_skipped_tokens.set(0);

			g_cycles_per_ms = cycle_count() / cycle_metric_t(get_time_ms());
			m_dragging = false;
			m_selecting = false;
			m_mouse_down_x = 0;
			m_mouse_down_y = 0;
			
			m_selection_a = 0;
			m_selection_b = 0;

			m_paused = false;

			m_token_dummy.name = "!!! DUMMY INSIGHT TOKEN !!!";
		}
		~Manager()
		{
			WaitForSingleObject(g_thread, INFINITE);
			CloseHandle(g_thread);
			for( size_t i=0; i<m_temporary_strings.size(); ++i )
			{
				free(m_temporary_strings[i]);
			}
		}
		
		void start_gui()
		{
			HINSTANCE hinst = GetModuleHandle(NULL);

			WNDCLASSEX wc; 
			wc.cbSize        = sizeof(WNDCLASSEX);
			wc.style         = CS_DBLCLKS;
			wc.lpfnWndProc   = wndproc;
			wc.cbClsExtra    = 0;
			wc.cbWndExtra    = 0;
			wc.hInstance     = hinst;
			wc.hIcon         = LoadIcon (NULL, IDI_APPLICATION);
			wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
			wc.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH);
			wc.lpszMenuName  = NULL;
			wc.lpszClassName = "INSIGHTPROFILERWC";
			wc.hIconSm       = wc.hIcon;

			RegisterClassEx(&wc);

			DWORD	window_style = WS_OVERLAPPEDWINDOW;

			m_hwnd = CreateWindowA("INSIGHTPROFILERWC", "Insight", window_style, 10, 10, 800, 400, NULL, NULL, hinst, NULL); 

			ShowWindow(m_hwnd, SW_SHOWNORMAL);
			UpdateWindow(m_hwnd);
		}

		void init()
		{
			g_thread = CreateThread(0, 0, threadproc, 0, 0, 0);
		}

		void destroy()
		{
			DestroyWindow(m_hwnd);
		}

		__forceinline Token* allocate_token(const char* name)
		{
			Token* e = m_token_pool.alloc();
			
			if( e )
			{
				e->name = name;
				e->time_start = cycle_count();
				e->thread_id = GetCurrentThreadId();
			}
			else
			{
				e = &m_token_dummy;
				m_skipped_tokens.inc();
			}

			return e;
		}
		__forceinline void free_token(Token* e)
		{
			if( e != &m_token_dummy )
			{
				e->time_stop = cycle_count();
				if( m_token_buffer.push(*e) == false ) 
				{
					m_skipped_tokens.inc();
				}
				m_token_pool.free(e);
			}
		}

		double get_time_ms()
		{
			LARGE_INTEGER freq;
			LARGE_INTEGER time;

			QueryPerformanceFrequency(&freq);
			QueryPerformanceCounter(&time);

			double result = double(1000*time.QuadPart) / double(freq.QuadPart);

			return result;
		}

		void update_timing()
		{
			double curr_time = get_time_ms();
			static double  s_prev_time = curr_time;
			double elapsed_time = curr_time-s_prev_time;

			cycle_metric_t curr_cycles = cycle_count();
			static cycle_metric_t s_prev_cycles = curr_cycles;
			cycle_metric_t elapsed_cycles = curr_cycles - s_prev_cycles;

			static double time_threshold = 0;

			if( elapsed_time > time_threshold ) // recalculate cycles per second approximately every 5 seconds 
			{
				g_cycles_per_ms = cycle_metric_t(elapsed_cycles / elapsed_time);
				s_prev_cycles = curr_cycles;
				s_prev_time = curr_time;

				if( time_threshold < 60000 )
				{
					time_threshold = time_threshold*1.1 + 1;
				}
			}
		}

		void pause()
		{
			m_paused = true;
		}

		void resume()
		{
			m_paused = false;
		}

		void load_report(const char* filename)
		{	
			m_ui.reset();

			for( size_t i=0; i<m_temporary_strings.size(); ++i )
			{
				free(m_temporary_strings[i]);
			}
			m_temporary_strings.clear();

			HANDLE hf = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if( hf!=INVALID_HANDLE_VALUE )
			{	
				long count;
				DWORD br;
				ReadFile(hf, &count, sizeof(count), &br, NULL);

				m_token_back_buffer.pos.val = count;

				for( long i=0; i<count; ++i )
				{
					Token& t = m_token_back_buffer.data[i];

					ReadFile(hf, &t.time_start, sizeof(t.time_start), &br, NULL);
					ReadFile(hf, &t.time_stop,  sizeof(t.time_stop),  &br, NULL);
					ReadFile(hf, &t.thread_id,  sizeof(t.thread_id),  &br, NULL);

					long name_len = 0;
					ReadFile(hf, &name_len, sizeof(name_len), &br, NULL);

					char* tmp_str = (char*)malloc(name_len+1);
					m_temporary_strings.push_back(tmp_str);

					memset(tmp_str, 0, name_len+1);

					ReadFile(hf, tmp_str, name_len, &br, NULL);

					t.name = tmp_str;
				}

				ReadFile(hf, &g_cycles_per_ms, sizeof(g_cycles_per_ms), &br, NULL);

				CloseHandle(hf);
			}
			build_report();

			InvalidateRect(m_hwnd, NULL, false);
		}

		void dump_report(const char* filename)
		{
			HANDLE hf = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if( hf!=INVALID_HANDLE_VALUE )
			{					
				long count = m_token_back_buffer.pos.val;
				DWORD bw;
				WriteFile(hf, &count, sizeof(count), &bw, NULL);
				
				for( long i=0; i<count; ++i )
				{
					Token& t = m_token_back_buffer.data[i];
					
					WriteFile(hf, &t.time_start, sizeof(t.time_start), &bw, NULL);
					WriteFile(hf, &t.time_stop, sizeof(t.time_stop), &bw, NULL);
					WriteFile(hf, &t.thread_id, sizeof(t.thread_id), &bw, NULL);
					
					long name_len = long(strlen(t.name));
					WriteFile(hf, &name_len, sizeof(name_len), &bw, NULL);
					WriteFile(hf, t.name, name_len, &bw, NULL);
				}
				
				WriteFile(hf, &g_cycles_per_ms, sizeof(g_cycles_per_ms), &bw, NULL);
				
				CloseHandle(hf);
			}
		}

		void build_report()
		{
			m_ui.reset();
			m_text.reset();

			long num_tokens = m_token_back_buffer.pos.val;

			if( num_tokens )
			{
				std::sort(&m_token_back_buffer.data[0], &m_token_back_buffer.data[num_tokens], TokenSorter());
			}

			size_t	thread_idx  = size_t(-1);
			DWORD	curr_thread = DWORD(-1);

			size_t num_bars = 0;
			Stack<Token, 64> call_stack;

			cycle_metric_t min_time = cycle_metric_t(-1);
			cycle_metric_t max_time = 0;
			for( long i=0; i<num_tokens; ++i )
			{
				Token& t = m_token_back_buffer.data[i];
				if( t.time_start < min_time ) min_time = t.time_start;
				if( t.time_stop > max_time ) max_time = t.time_stop;
			}
			m_ui.set_timeframe(min_time, max_time, g_cycles_per_ms);

			for( long i=0; i<num_tokens; ++i )
			{
				Token& t = m_token_back_buffer.data[i];

				if( curr_thread != t.thread_id )
				{
					curr_thread = t.thread_id;
					++thread_idx;
					call_stack.reset();
				}

				while( call_stack.size() && t.time_start > call_stack.peek().time_stop  )
				{
					call_stack.pop();
				}

				if( call_stack.size()==0 || t.time_stop < call_stack.peek().time_stop )
				{
					call_stack.push(t);
				}

				if( (DWORD)t.name < 0x00010000 )
				{
					DebugBreak();
				}

				m_ui.add_bar(thread_idx, call_stack.size()-1, t);
			}
		}

		void snapshot()
		{
			long num_tokens = std::min(long(MAX_PROFILE_TOKENS), m_token_buffer.lock());
			memcpy(&m_token_back_buffer, &m_token_buffer, sizeof(m_token_buffer));
			m_token_buffer.flush();
			m_token_back_buffer.pos.set(num_tokens);
			build_report();
		}

		LRESULT process_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
		{
			HDC hdc;
			PAINTSTRUCT ps;
			RECT client_rect;
			RECT invalid_rect={-1,-1,-1,-1};

			GetClientRect(hwnd, &client_rect);

			RECT text_rect  = client_rect;
			RECT graph_rect = client_rect;

			graph_rect.bottom = client_rect.bottom - 40;
			text_rect.top = graph_rect.bottom;
			
			bool active = m_hwnd != GetForegroundWindow();

			switch(msg)
			{	
			case WM_KEYDOWN:
				if(wparam == 'S')
				{
					dump_report("dump.insight");
				}
				return 0;
			case WM_CREATE:
				SetTimer(hwnd, WM_TIMER, 1000, NULL);
				m_ui.init(hwnd);
				return 0;

			case WM_PAINT:
				{
					hdc = BeginPaint(hwnd, &ps);
					m_text.reset();

					if( m_ui.selection_length()>0 )
					{
						m_text.print("Range: %.4f ms\n", m_ui.selection_length());
					}
					if( m_ui.selected() )
					{
						m_text.print("Selected: %s\n", m_ui.selected()->name);
						float duration = m_ui.selected()->x2 - m_ui.selected()->x1;
						m_text.print("Duration: %.4f ms\n", duration);
					}
					if( active==false && m_paused==false )
					{
						m_text.print("PAUSED. Change focus to another window to resume.\n");
					}

					m_text.draw(hdc,text_rect);
					EndPaint (hwnd, &ps);

					m_ui.render();
 				}
				return 0;

			case WM_TIMER:
				if( active && m_paused==false )
				{
					snapshot();
					update_timing();
					build_report();
				}
				InvalidateRect(hwnd, &text_rect, false);
				return 0;

			case WM_SIZE:
				m_ui.resize(graph_rect);
				InvalidateRect(hwnd, NULL, false);
				return 0;
			
			case WM_MOUSEWHEEL:
				m_ui.zoom(GET_WHEEL_DELTA_WPARAM(wparam));
				InvalidateRect(hwnd, &text_rect, false);
				return 0;

			case WM_MOUSEMOVE:
				if( m_selecting )
				{
					m_selection_b = LOWORD(lparam);
					m_ui.select(m_selection_a, m_selection_b);
				}
				else if( m_dragging )
				{
					m_ui.drag(LOWORD(lparam) - m_mouse_down_x, HIWORD(lparam) - m_mouse_down_y);
					m_mouse_down_x = LOWORD(lparam);
					m_mouse_down_y = HIWORD(lparam);
				}
				m_ui.set_cursor(LOWORD(lparam), HIWORD(lparam));
				InvalidateRect(hwnd, &text_rect, false);
				return 0;

			case WM_LBUTTONDOWN:
				m_dragging = true;
				m_mouse_down_x = LOWORD(lparam);
				m_mouse_down_y = HIWORD(lparam);
				return 0;

			case WM_RBUTTONDOWN:
				m_selecting = true;
				m_selection_a = LOWORD(lparam);
				m_selection_b = m_selection_a;
				return 0;

			case WM_LBUTTONUP:
				m_dragging = false;
				return 0;

			case WM_RBUTTONUP:
				m_selecting = false;
				m_ui.select(m_selection_a, m_selection_b);
				InvalidateRect(m_hwnd, 0, FALSE);
				return 0;

			case WM_CLOSE:
				TerminateProcess(GetCurrentProcess(), 0);
				return 0;

			case WM_DESTROY:
				m_ui.destroy();
				return 0;
			
			default:
				return (LONG)DefWindowProc(hwnd, msg, wparam, lparam);
			}

		}

	private:

		HWND			m_hwnd;

		Token			m_token_dummy;
		
		TokenPool		m_token_pool;
		TokenBuffer		m_token_buffer;
		TokenBuffer		m_token_back_buffer;

		AtomicInt			m_skipped_tokens;

		Text<MAX_REPORT_TEXT>	m_text;
		
		InsightGraphD3D9 m_ui;

		Mutex m_report_lock;

		bool m_dragging;
		bool m_selecting;
		long m_mouse_down_x;
		long m_mouse_down_y;
		long m_selection_a;
		long m_selection_b;
		bool m_paused;
		std::vector<char*> m_temporary_strings;
	};

	Manager g_instance;

	Token* enter(const char* n)
	{
		return g_instance.allocate_token(n);
	}

	void exit( Token* e )
	{
		g_instance.free_token( e );
	}

	//////////////////////////////////////////////////////////////////////////	

	LRESULT APIENTRY wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		return g_instance.process_message(hwnd, msg, wparam, lparam);
	}
	DWORD WINAPI threadproc(void*)
	{
		g_instance.start_gui();

		bool should_continue = true;
		while(should_continue)
		{
			MSG msg;
			while( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if( msg.message == WM_QUIT )
				{
					should_continue = false;
				}
			}
			Sleep(1);
		}
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////

	Initializer::Initializer()
	{
		g_instance.init();
	}

	Initializer::~Initializer()
	{
		g_instance.destroy();
	}

}


#include "insight_gui.h"

#include "insight/redist/insight.h"
#include "insight_graph_d3d9.h"
#include "insight_backend.h"

#define NOMINMAX
#include <stdio.h>
#include <Windows.h>
#include <algorithm>
#include <vector>


#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif //GET_WHEEL_DELTA_WPARAM

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif //WM_MOUSEWHEEL

namespace
{
	const size_t MAX_REPORT_TEXT =	2048;

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

	bool g_should_exit = false;
	bool g_asynchronous = false;
	std::vector<char*> g_temporary_strings;
	HANDLE	g_thread = 0;
	Text<MAX_REPORT_TEXT> g_text;

	InsightGui::GraphD3D9	g_graph;
	HWND					g_hwnd;
	

	bool g_dragging;
	bool g_selecting;
	long g_mouse_down_x;
	long g_mouse_down_y;
	long g_selection_a;
	long g_selection_b;
	bool g_paused;
	bool g_start_minimized;

	Insight::cycle_metric g_cycles_per_ms = 1;

	//////////////////////////////////////////////////////////////////////////

	float cycles_to_ms(Insight::cycle_metric cycles)
	{
		float res = float (double(cycles) / double(g_cycles_per_ms));
		return res;
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

		Insight::cycle_metric curr_cycles = __rdtsc();
		static Insight::cycle_metric s_prev_cycles = curr_cycles;
		Insight::cycle_metric elapsed_cycles = curr_cycles - s_prev_cycles;

		static double time_threshold = 0;

		if( elapsed_time > time_threshold ) // recalculate cycles per second approximately every 5 seconds 
		{
			g_cycles_per_ms = Insight::cycle_metric(elapsed_cycles / elapsed_time);
			s_prev_cycles = curr_cycles;
			s_prev_time = curr_time;

			if( time_threshold < 60000 )
			{
				time_threshold = time_threshold*1.1 + 1;
			}
		}
	}


	struct TokenSorter
	{
		bool operator()(const Insight::Token& a, const Insight::Token& b)
		{
			if( a.thread_id == b.thread_id )
			{
				return a.time_enter < b.time_enter;
			}
			else
			{
				return a.thread_id < b.thread_id;
			}
		}
	};


	void build_report()
	{
		long num_tokens = InsightBackend::g_token_back_buffer.pos.val;

		if( num_tokens )
		{
			std::sort(&InsightBackend::g_token_back_buffer.data[0], &InsightBackend::g_token_back_buffer.data[num_tokens], TokenSorter());
		}

		size_t thread_idx  = size_t(-1);
		unsigned long curr_thread = unsigned long(-1);

		InsightUtils::Stack<Insight::Token, 64> call_stack;

		Insight::cycle_metric min_time = Insight::cycle_metric(-1);
		Insight::cycle_metric max_time = 0;
		for( long i=0; i<num_tokens; ++i )
		{
			Insight::Token& t = InsightBackend::g_token_back_buffer.data[i];
			if( t.time_enter < min_time ) min_time = t.time_enter;
			if( t.time_exit > max_time ) max_time = t.time_exit;
		}
		g_graph.set_timeframe(min_time, max_time, g_cycles_per_ms);

		for( long i=0; i<num_tokens; ++i )
		{
			Insight::Token& t = InsightBackend::g_token_back_buffer.data[i];

			if( curr_thread != t.thread_id )
			{
				curr_thread = t.thread_id;
				++thread_idx;
				call_stack.reset();
			}

			while( call_stack.size() && t.time_enter > call_stack.peek().time_exit  )
			{
				call_stack.pop();
			}

			if( call_stack.size()==0 || t.time_exit < call_stack.peek().time_exit )
			{
				call_stack.push(t);
			}

			g_graph.add_bar(thread_idx, call_stack.size()-1, t);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	LRESULT APIENTRY wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		HDC hdc;
		PAINTSTRUCT ps;
		RECT client_rect;

		GetClientRect(hwnd, &client_rect);

		RECT text_rect  = client_rect;
		RECT graph_rect = client_rect;

		graph_rect.bottom = client_rect.bottom - 40;
		text_rect.top = graph_rect.bottom;

		bool active = g_hwnd != GetForegroundWindow();

		switch(msg)
		{
		case WM_CREATE:
			SetTimer(hwnd, WM_TIMER, 1000, NULL);
			g_graph.init(hwnd);
			return 0;

		case WM_PAINT:
			{
				hdc = BeginPaint(hwnd, &ps);
				g_text.reset();

				if( g_graph.selection_length()>0 )
				{
					g_text.print("Range: %.4f ms\n", g_graph.selection_length());
				}
				if( g_graph.selected() )
				{
					g_text.print("Selected: %s\n", g_graph.selected()->name);
					float duration = g_graph.selected()->x2 - g_graph.selected()->x1;
					g_text.print("Duration: %.4f ms\n", duration);
				}
				if( active==false && g_paused==false )
				{
					g_text.print("PAUSED. Change focus to another window to resume.\n");
				}

				g_text.draw(hdc,text_rect);
				EndPaint (hwnd, &ps);

				g_graph.render();
			}
			return 0;

		case WM_TIMER:
			if( active && g_paused==false )
			{
				InsightBackend::snapshot();

				update_timing();
				g_graph.reset();
				g_text.reset();
				build_report();
			}
			InvalidateRect(hwnd, &text_rect, false);
			return 0;

		case WM_SIZE:
			g_graph.resize(graph_rect);
			InvalidateRect(hwnd, NULL, false);
			return 0;

		case WM_MOUSEWHEEL:
			g_graph.zoom(GET_WHEEL_DELTA_WPARAM(wparam));
			InvalidateRect(hwnd, &text_rect, false);
			return 0;

		case WM_MOUSEMOVE:
			if( g_selecting )
			{
				g_selection_b = LOWORD(lparam);
				g_graph.select(g_selection_a, g_selection_b);
			}
			else if( g_dragging )
			{
				g_graph.drag(LOWORD(lparam) - g_mouse_down_x);
				g_mouse_down_x = LOWORD(lparam);
				g_mouse_down_y = HIWORD(lparam);
			}
			g_graph.set_cursor(LOWORD(lparam), HIWORD(lparam));
			InvalidateRect(hwnd, &text_rect, false);
			return 0;

		case WM_LBUTTONDOWN:
			g_dragging = true;
			g_mouse_down_x = LOWORD(lparam);
			g_mouse_down_y = HIWORD(lparam);
			return 0;

		case WM_RBUTTONDOWN:
			g_selecting = true;
			g_selection_a = LOWORD(lparam);
			g_selection_b = g_selection_a;
			return 0;

		case WM_LBUTTONUP:
			g_dragging = false;
			return 0;

		case WM_RBUTTONUP:
			g_selecting = false;
			g_graph.select(g_selection_a, g_selection_b);
			InvalidateRect(g_hwnd, 0, FALSE);
			return 0;

		case WM_CLOSE:
			TerminateProcess(GetCurrentProcess(), 0);
			return 0;

		case WM_DESTROY:
			g_graph.destroy();
			return 0;

		default:
			return (LONG)DefWindowProc(hwnd, msg, wparam, lparam);
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

		g_hwnd = CreateWindow("INSIGHTPROFILERWC", "Insight", window_style, 10, 10, 800, 400, NULL, NULL, hinst, NULL); 

		if( g_start_minimized )
		{
			ShowWindow(g_hwnd, SW_SHOWNOACTIVATE|SW_SHOWMINIMIZED);
		}
		else
		{
			ShowWindow(g_hwnd, SW_SHOWNORMAL);
		}
		
		UpdateWindow(g_hwnd);
	}

	bool update_gui_window()
	{
		bool should_continue = true;
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

		return should_continue;
	}

	DWORD WINAPI threadproc(void*)
	{
		start_gui();
		bool should_continue = true;
		while(should_continue)
		{
			if( g_should_exit )
			{
				DestroyWindow(g_hwnd);
				break;
			}
			else
			{
				should_continue = update_gui_window();
				Sleep(1);
			}
		}
		return 0;
	}
}

namespace InsightGui
{
	void initialize(bool asynchronous, bool start_minimized)
	{
		g_asynchronous = asynchronous;
		g_cycles_per_ms = __rdtsc() / Insight::cycle_metric(get_time_ms());
		g_dragging = false;
		g_selecting = false;
		g_mouse_down_x = 0;
		g_mouse_down_y = 0;

		g_selection_a = 0;
		g_selection_b = 0;

		g_paused = false;

		g_start_minimized = start_minimized;

		if( g_asynchronous )
		{
			g_thread = CreateThread(0, 0, threadproc, 0, 0, 0);
		}
		else
		{
			start_gui();
		}

	}

	void terminate()
	{
		if( g_asynchronous )
		{
			g_should_exit = true;
			WaitForSingleObject(g_thread, 10000);
			CloseHandle(g_thread);
		}
		else
		{
			DestroyWindow(g_hwnd);
		}
		for( size_t i=0; i<g_temporary_strings.size(); ++i )
		{
			free(g_temporary_strings[i]);
		}
	}

	extern void update()
	{
		if( g_asynchronous == false )
		{
			update_gui_window();
		}
	}
}


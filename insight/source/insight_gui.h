#ifndef __INSIGHT_GUI_H__
#define __INSIGHT_GUI_H__

#define NOMINMAX
#include <Windows.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <d3d9types.h>
#include <DxErr.h>

#include "insight/redist/insight.h"

#include "insight_utils.h"

#pragma warning(disable:4100)

namespace Insight
{

	class ColourManager
	{
	public:
		ColourManager()
			: m_last_colour(0)
			, m_last_string(NULL)
		{}
		
		DWORD			calculate_colour(const char* str);
	
	private:

		DWORD			m_last_colour;
		const char*		m_last_string;
	};

	class InsightGraphD3D9
	{	
	public:

		struct BarInfo
		{
			const char* name;
			float x1,x2,y1,y2;
			size_t thread_idx;
		};

		struct Vertex
		{
			Vertex(){};
			Vertex(float _x, float _y, float _z, float _u, float _v, DWORD _colour)
				: x(_x), y(_y), z(_z), u(_u), v(_v), colour(_colour)
			{}
			float x,y,z;
			DWORD colour;
			float u,v;
		};

	public:

		InsightGraphD3D9();
		~InsightGraphD3D9();

		void init(HWND hwnd);
		void destroy();
		bool handle_device_lost();
		void set_render_states();

		void resize(RECT rect);
		void render();

		void draw_rect(float x1, float y1, float x2, float y2, DWORD colour);
		
		void set_timeframe(cycle_metric min_time, cycle_metric max_time, cycle_metric cycles_per_ms);
		
		void zoom(float delta_z);
		void drag(long x, long y);
		void select(long a, long b);
		void set_cursor(long cx, long cy);

		void reset();
		bool add_bar(size_t thread_idx, size_t depth, const Token& t);

		void update_viewport();

		BarInfo* selected();
		float	 selection_length();

		void draw_temporary_objects();

		void client_to_time(long cx, long cy, float& tx, float& ty);

	private:
		
		IDirect3D9*				m_d3d9;
		IDirect3DDevice9*		m_device;
		IDirect3DTexture9*		m_tex_background;
		IDirect3DTexture9*		m_tex_bar;
		D3DPRESENT_PARAMETERS	m_present_paramseters;
		bool					m_device_lost;

		RECT					m_rect;
		HWND					m_hwnd;

		float					m_zoom;

		float					m_pos_x;
		float					m_pos_y;

		cycle_metric 			m_min_time;
		cycle_metric 			m_max_time;
		cycle_metric 			m_cycles_per_ms;

		static const size_t MAX_BARS=16384;
		Stack<BarInfo, MAX_BARS>	m_bars;
		Stack<Vertex,  MAX_BARS*6>	m_bar_vertices;
		Stack<Vertex,  32>			m_tmp_vertices;
		Stack<Vertex,  32>			m_bg_vertices;

		size_t m_max_thread_idx;

		float m_cursor_x;
		float m_cursor_y;
		float m_selection_a;
		float m_selection_b;

		BarInfo* m_selected;

		D3DXMATRIX m_transform_bg;
		D3DXMATRIX m_transform;
		D3DXMATRIX m_transform_inverse;

		ColourManager m_colourman;
	};

}
#endif // __INSIGHT_GUI_H__


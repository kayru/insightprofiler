#include "insight/redist/insight.h"
#include "insight_graph_d3d9.h"
#include "insight_background_png.h"
#include "insight_bar_png.h"

#include <stdio.h>
#include <algorithm>

#pragma warning(disable:4702)
#pragma warning(disable:4189)

namespace
{
	static bool check_result( HRESULT hr )
	{
		if( FAILED(hr) && hr!=D3DERR_DEVICELOST && hr!=D3DERR_DEVICENOTRESET )
		{
			//TODO: Show some kind of error message
			return false;
		}
		else
		{
			return true;
		}
	}
}

namespace InsightGui
{

	const DWORD g_fvf = D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
	const float g_bar_height	= 100.0f;
	const float g_thread_height = g_bar_height + 10.0f;
	const float g_stack_height  = g_thread_height/10.0f;
	
	//////////////////////////////////////////////////////////////////////////

	DWORD ColourManager::calculate_colour(const char* str)
	{
		if( m_last_string == str )
		{
			return m_last_colour;
		}
		else
		{
			// this is a "one-at-a-time" hash
			DWORD hash = 0x811c9dc5;
			for( size_t i=0; str[i]!=0 && i<128; ++i )
			{	
				hash += str[i];
				hash += hash<<10;
				hash ^= hash>>6;
			}
			hash += hash<<3;
			hash ^= hash>>11;
			hash += hash<<15;

			DWORD colour = 0xFF000000 | hash;

			m_last_string = str;
			m_last_colour = colour;

			return colour;
		}
	}

	//////////////////////////////////////////////////////////////////////////

	GraphD3D9::GraphD3D9()
		: m_d3d9(NULL)
		, m_device(NULL)
		, m_device_lost(false)
		, m_rect()
		, m_zoom(500)
		, m_pos_x(250)
		, m_pos_y(0)
		, m_cursor_x(0)
		, m_cursor_y(0)
		, m_selection_a(0)
		, m_selection_b(0)
		, m_min_time(0)
		, m_max_time(1)
		, m_cycles_per_ms(1)
		, m_max_thread_idx(0)
		, m_selected(NULL)
	{
		m_rect.left   = 0;
		m_rect.top    = 0;
		m_rect.right  = 1;
		m_rect.bottom = 1;
		D3DXMatrixIdentity(&m_transform);
		D3DXMatrixIdentity(&m_transform_inverse);
		D3DXMatrixIdentity(&m_transform_bg);
	}

	GraphD3D9::~GraphD3D9()
	{

	}

	void GraphD3D9::render()
	{		
		// attempt to fix lost device and quit if was not able
		bool ok_to_render = handle_device_lost();
		if( ok_to_render == false ) 
		{
			return;
		}
		
		update_viewport();

		draw_temporary_objects();

		check_result(m_device->BeginScene());

		check_result(m_device->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 0xFF111111, 1.0f, 0));

		size_t vertex_size = sizeof(Vertex);

		//draw background
		check_result(m_device->SetTransform(D3DTS_PROJECTION, &m_transform_bg));
		check_result(m_device->SetTexture(0, m_tex_background));
		if( m_bg_vertices.size()/3 > 0 )
		{
			check_result(m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, m_bg_vertices.size()/3, m_bg_vertices.data, vertex_size));
		}

		check_result(m_device->SetTransform(D3DTS_PROJECTION, &m_transform));
		// draw bars
		check_result(m_device->SetTexture(0, m_tex_bar));
		if( m_bar_vertices.size()/3 > 0 )
		{
			check_result(m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, m_bar_vertices.size()/3, m_bar_vertices.data, vertex_size));
		}

		// draw selection
		if( m_tmp_vertices.size()/3 > 0 )
		{
			check_result(m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, m_tmp_vertices.size()/3, m_tmp_vertices.data, vertex_size));
		}

		check_result(m_device->EndScene());
		
		HRESULT hr = m_device->Present(&m_rect, &m_rect, 0, 0);
		
		if( hr == D3DERR_DEVICELOST )
		{
			m_device_lost = true;
		}
		else
		{
			check_result(hr);
		}
	}

	void GraphD3D9::resize( RECT rect )
	{
		m_rect = rect;
	}

	void GraphD3D9::init( HWND hwnd )
	{
		m_hwnd = hwnd;
		m_d3d9 = Direct3DCreate9( D3D_SDK_VERSION );

		D3DDISPLAYMODE dm;
		ZeroMemory(&dm, sizeof(dm));

		D3DPRESENT_PARAMETERS pp;
		ZeroMemory(&pp, sizeof(pp));

		DWORD		adapter				= D3DADAPTER_DEFAULT;
		D3DDEVTYPE	type				= D3DDEVTYPE_HAL;

		DWORD bb_max_width  = 2048;
		DWORD bb_max_height = 2048;

		check_result( m_d3d9->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &dm ) );
		
		pp.BackBufferWidth				= bb_max_width;
		pp.BackBufferHeight				= bb_max_height;
		pp.BackBufferFormat 			= dm.Format;
		pp.BackBufferCount				= 1;

		pp.MultiSampleType				= D3DMULTISAMPLE_NONE;
		pp.MultiSampleQuality			= 0;

		pp.SwapEffect					= D3DSWAPEFFECT_COPY;
		pp.hDeviceWindow				= hwnd;
		pp.Windowed						= TRUE;
		pp.EnableAutoDepthStencil		= TRUE;
		pp.AutoDepthStencilFormat		= D3DFMT_D24S8;
		pp.Flags						= 0;

		pp.FullScreen_RefreshRateInHz	= 0;
		pp.PresentationInterval			= D3DPRESENT_INTERVAL_IMMEDIATE;

		memcpy(&m_present_paramseters, &pp, sizeof(m_present_paramseters));

		DWORD flags						= D3DCREATE_HARDWARE_VERTEXPROCESSING;

		check_result(m_d3d9->CreateDevice(adapter, type, hwnd, flags, &pp, &m_device));

		check_result(D3DXCreateTextureFromFileInMemoryEx(
			m_device, g_background_png, sizeof(g_background_png), 
			D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, 
			D3DX_DEFAULT, D3DX_DEFAULT, 0, 0, 0, &m_tex_background));

		check_result(D3DXCreateTextureFromFileInMemoryEx(
			m_device, g_bar_png, sizeof(g_bar_png), 
			D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, 
			D3DX_DEFAULT, D3DX_DEFAULT, 0, 0, 0, &m_tex_bar));

		set_render_states();
	}

	void GraphD3D9::destroy()
	{
		if(m_tex_background)	m_tex_background->Release();
		if(m_tex_bar)			m_tex_bar->Release();
		if(m_device)			m_device->Release();
		if(m_d3d9)				m_d3d9->Release();
	}
	
	void GraphD3D9::draw_rect( float x1, float y1, float x2, float y2, DWORD colour )
	{
		m_bar_vertices.push(Vertex(x1, y1, 0.0f, 0, 0, colour));
		m_bar_vertices.push(Vertex(x2, y1, 0.0f, 1, 0, colour));
		m_bar_vertices.push(Vertex(x2, y2, 0.0f, 1, 1, colour));

		m_bar_vertices.push(Vertex(x1, y1, 0.0f, 0, 0, colour));
		m_bar_vertices.push(Vertex(x2, y2, 0.0f, 1, 1, colour));
		m_bar_vertices.push(Vertex(x1, y2, 0.0f, 0, 1, colour));
	}

	void GraphD3D9::zoom( float delta_z )
	{
		m_zoom -= delta_z * m_zoom * 0.001f;

		if( m_zoom < 0.01f   ) m_zoom = 0.01f;
		if( m_zoom > 2000.0f ) m_zoom = 2000.0f;
	}
	
	bool GraphD3D9::add_bar( size_t thread_idx, size_t depth, const Insight::Token& t )
	{
		m_max_thread_idx = std::max(thread_idx, m_max_thread_idx);
				
		float duration_ms	= float(double(t.time_exit - t.time_enter) / double(m_cycles_per_ms));
		float start_ms		= float(double(t.time_enter - m_min_time) / double(m_cycles_per_ms));
		float stop_ms		= float(double(t.time_exit - m_min_time) / double(m_cycles_per_ms));

		float pos_x1 = start_ms;
		float pos_x2 = stop_ms;

		float pos_y1 = float(thread_idx) * g_thread_height + float(depth)*g_stack_height;
		float pos_y2 = (pos_y1 + g_bar_height) - float(depth)*g_stack_height;

		DWORD colour = m_colourman.calculate_colour(t.name);
		
		draw_rect(pos_x1, pos_y1, pos_x2, pos_y2, colour);

		BarInfo bi;
		bi.x1 = pos_x1;
		bi.x2 = pos_x2;
		bi.y1 = pos_y1;
		bi.y2 = pos_y2;
		bi.thread_idx = thread_idx;
		bi.name = t.name;

		m_bars.push(bi);

		return true;
	}

	void GraphD3D9::reset()
	{
		m_bars.reset();

		m_bar_vertices.reset();

		m_max_thread_idx = 0;

		m_selected = NULL;
		m_tmp_vertices.reset();

		m_selection_a = 0;
		m_selection_b = 0;
	}

	void GraphD3D9::set_timeframe( Insight::cycle_metric min_time, Insight::cycle_metric max_time, Insight::cycle_metric cycles_per_ms )
	{
		m_min_time = min_time;
		m_max_time = max_time;
		m_cycles_per_ms = cycles_per_ms;
	}

	void GraphD3D9::drag( long x, long y )
	{
		float width = float(m_rect.right - m_rect.left);
		m_pos_x -= float(x) * (m_zoom/width);
		if( m_pos_x < 0 ) m_pos_x = 0;
	}

	void GraphD3D9::select( long a, long b )
	{		
		float unused;
		client_to_time(a,0,m_selection_a,unused);
		client_to_time(b,0,m_selection_b,unused);
	}


	void GraphD3D9::update_viewport()
	{
		D3DVIEWPORT9 vp;

		vp.X = m_rect.left;
		vp.Y = m_rect.top;
		vp.MinZ = 0;
		vp.MaxZ = 1;
		vp.Width  = std::max(LONG(1), std::min(LONG(2048),m_rect.right-m_rect.left) );
		vp.Height = std::max(LONG(1), std::min(LONG(2048),m_rect.bottom-m_rect.top) );

		check_result(m_device->SetViewport(&vp));
		
		D3DXMATRIXA16 mat_tmp;

		D3DXMatrixIdentity(&m_transform);
		D3DXMatrixIdentity(&mat_tmp);

		D3DXMatrixScaling(&m_transform, 1.0f, -1.0f, 1.0f);
		D3DXMatrixTranslation(&mat_tmp, -0.5f, +0.5f, 0.0f);
		D3DXMatrixMultiply(&m_transform, &m_transform, &mat_tmp);

		float x1 = m_pos_x - m_zoom/2;
		float x2 = m_pos_x + m_zoom/2;
		float y1 = 0;
		float y2 = -g_thread_height*float(m_max_thread_idx+1);
		
		D3DXMatrixOrthoOffCenterLH(&mat_tmp, x1, x2, y2, y1, vp.MinZ, vp.MaxZ);
		D3DXMatrixMultiply(&m_transform, &m_transform, &mat_tmp);

		D3DXMatrixInverse(&m_transform_inverse, NULL, &m_transform);
		
		m_bg_vertices.reset();
		
		float u = float(vp.Width)/6;
		float v = float(vp.Height)/6;

		float bx1 = -1 - 0.5f/u;
		float bx2 =  1 - 0.5f/u;
		float by1 =  1 + 0.5f/v;
		float by2 = -1 + 0.5f/v;

		DWORD bg_col = 0xFF808080;
		
		m_bg_vertices.push(Vertex(bx1, by1, 0.0f, 0, 0, bg_col));
		m_bg_vertices.push(Vertex(bx2, by1, 0.0f, u, 0, bg_col));
		m_bg_vertices.push(Vertex(bx2, by2, 0.0f, u, v, bg_col));
		m_bg_vertices.push(Vertex(bx1, by1, 0.0f, 0, 0, bg_col));
		m_bg_vertices.push(Vertex(bx2, by2, 0.0f, u, v, bg_col));
		m_bg_vertices.push(Vertex(bx1, by2, 0.0f, 0, v, bg_col));

	}

	void GraphD3D9::set_cursor( long cx, long cy )
	{				
		float w = float(m_rect.right-m_rect.left);
		float h = float(m_rect.bottom-m_rect.top);

		float x = (float(cx) / w)*2-1;
		float y = (float(cy) / h)*2-1;

		D3DXVECTOR4 v1(x,-y,0,1);
		D3DXVECTOR4 v2;

		D3DXVec4Transform(&v2,&v1,&m_transform_inverse);

		m_cursor_x = v2.x;
		m_cursor_y = v2.y;

		m_selected = NULL;
		for(size_t i=0; i<m_bars.size(); ++i)
		{
			BarInfo& bi = m_bars[i];
			if( m_cursor_x >= bi.x1 && m_cursor_x <= bi.x2 &&
				m_cursor_y >= bi.y1 && m_cursor_y <= bi.y2 )
			{
				m_selected = &bi;
			}
		}
	}

	GraphD3D9::BarInfo* GraphD3D9::selected()
	{
		return m_selected;
	}

	bool GraphD3D9::handle_device_lost()
	{		
		if( m_device_lost )
		{
			HRESULT hr = m_device->TestCooperativeLevel();
			if( hr == D3D_OK )
			{
				m_device_lost = false;
				return true;
			}
			else if( hr == D3DERR_DEVICENOTRESET)
			{
				if( m_device->Reset(&m_present_paramseters) == D3D_OK )
				{
					set_render_states();
					m_device_lost = false;
					return true;
				}
			}
			return false;
		}
		else
		{
			return true;
		}
	}

	void GraphD3D9::set_render_states()
	{
		check_result(m_device->SetRenderState(D3DRS_LIGHTING, FALSE));
		check_result(m_device->SetRenderState(D3DRS_ZENABLE,  FALSE));
		check_result(m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));

		check_result(m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
		check_result(m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
		check_result(m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));

		check_result(m_device->SetTextureStageState(0, D3DTSS_COLOROP,		D3DTOP_MODULATE));
		check_result(m_device->SetTextureStageState(0, D3DTSS_COLORARG1,	D3DTA_TEXTURE));
		check_result(m_device->SetTextureStageState(0, D3DTSS_COLORARG2,	D3DTA_DIFFUSE));

		check_result(m_device->SetTextureStageState(0, D3DTSS_ALPHAOP,		D3DTOP_MODULATE));
		check_result(m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1,	D3DTA_TEXTURE ));
		check_result(m_device->SetTextureStageState(0, D3DTSS_ALPHAARG2,	D3DTA_DIFFUSE ));

		check_result(m_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
		check_result(m_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
		check_result(m_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR));

		check_result(m_device->SetFVF(g_fvf));
	}

	void GraphD3D9::draw_temporary_objects()
	{
		m_tmp_vertices.reset();
		if( m_selected )
		{
			float x1 = m_selected->x1;
			float x2 = m_selected->x2;
			float y1 = m_selected->y1;
			float y2 = m_selected->y2;

			m_tmp_vertices.push(Vertex(x1, y1, 0.0f, 0, 0, 0x88FFFFFF));
			m_tmp_vertices.push(Vertex(x2, y1, 0.0f, 1, 0, 0x88FFFFFF));
			m_tmp_vertices.push(Vertex(x2, y2, 0.0f, 1, 1, 0x88FFFFFF));

			m_tmp_vertices.push(Vertex(x1, y1, 0.0f, 0, 0, 0x88FFFFFF));
			m_tmp_vertices.push(Vertex(x2, y2, 0.0f, 1, 1, 0x88FFFFFF));
			m_tmp_vertices.push(Vertex(x1, y2, 0.0f, 0, 1, 0x88FFFFFF));
		}

		if( m_selection_a!=m_selection_b )
		{
			float x1 = m_selection_a;
			float x2 = m_selection_b;
			float y1 = 0;
			float y2 = 10000;

			DWORD col = 0x20FFFFFF;

			m_tmp_vertices.push(Vertex(x1, y1, 0.0f, 0, 0, col));
			m_tmp_vertices.push(Vertex(x2, y1, 0.0f, 1, 0, col));
			m_tmp_vertices.push(Vertex(x2, y2, 0.0f, 1, 1, col));

			m_tmp_vertices.push(Vertex(x1, y1, 0.0f, 0, 0, col));
			m_tmp_vertices.push(Vertex(x2, y2, 0.0f, 1, 1, col));
			m_tmp_vertices.push(Vertex(x1, y2, 0.0f, 0, 1, col));
		}
	}

	void GraphD3D9::client_to_time( long cx, long cy, float& tx, float& ty )
	{
		float w = float(m_rect.right-m_rect.left);
		float h = float(m_rect.bottom-m_rect.top);

		float x = (float(cx) / w)*2-1;
		float y = (float(cy) / h)*2-1;

		D3DXVECTOR4 v1(x,-y,0,1);
		D3DXVECTOR4 v2;

		D3DXVec4Transform(&v2,&v1,&m_transform_inverse);

		tx = v2.x;
		ty = v2.y;
	}

	float GraphD3D9::selection_length()
	{
		return abs(m_selection_b - m_selection_a);
	}
}


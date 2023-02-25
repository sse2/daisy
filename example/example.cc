// warning! no d3d9/winapi error handling in this **EXAMPLE**! this is meant to be a quick & dirty way to test stuff, with no guarantees.
#include "../daisy.hh"

#include <d3d9.h>
#include <tchar.h>

#pragma comment( lib, "d3d9.lib" )

inline LPDIRECT3D9 g_d3d = nullptr;
inline LPDIRECT3DDEVICE9 g_device = nullptr;
inline D3DPRESENT_PARAMETERS g_params { };

LRESULT WINAPI wnd_proc ( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
  switch ( msg )
  {
  case WM_DESTROY:
    ::PostQuitMessage ( 0 );
    return 0;
  }

  return ::DefWindowProc ( hWnd, msg, wParam, lParam );
}

INT WINAPI WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow )
{
  ( void ) hPrevInstance;
  ( void ) lpCmdLine;
  ( void ) nCmdShow;

  // create our window
  WNDCLASSEX wc = { sizeof ( WNDCLASSEX ), CS_CLASSDC, wnd_proc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, _T("DaisyExampleWndClass"), nullptr };
  RegisterClassEx ( &wc );
  HWND hwnd = CreateWindow ( wc.lpszClassName, _T("daisy example window"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr );

  // create d3d9 ctx
  g_d3d = Direct3DCreate9 ( D3D_SDK_VERSION );

  ZeroMemory ( &g_params, sizeof ( D3DPRESENT_PARAMETERS ) );
  g_params.Windowed = TRUE;
  g_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  g_params.BackBufferFormat = D3DFMT_UNKNOWN;
  g_params.EnableAutoDepthStencil = TRUE;
  g_params.AutoDepthStencilFormat = D3DFMT_D16;
  g_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

  // create d3d9 device
  g_d3d->CreateDevice ( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_params, &g_device );

  daisy::daisy_t::s_device = g_device;

  ShowWindow ( hwnd, SW_SHOWDEFAULT );
  UpdateWindow ( hwnd );

  // main window loop
  MSG msg;
  bool bail = false;

  daisy::c_renderqueue queue;
  queue.create ( );

  daisy::c_daisy_fontwrapper wrap;
  wrap.create ( "MS UI Gothic", 10, CLEARTYPE_NATURAL_QUALITY, daisy::FONT_DEFAULT );
  daisy::c_daisy_fontwrapper verd;
  verd.create ( "Comic Sans MS", 14, CLEARTYPE_NATURAL_QUALITY, daisy::FONT_DEFAULT );
  daisy::c_daisy_fontwrapper cmd;
  cmd.create ( "Arial Italic", 14, CLEARTYPE_NATURAL_QUALITY, daisy::FONT_DEFAULT );

  g_device->SetRenderState ( D3DRS_ZENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_ALPHABLENDENABLE, TRUE );
  g_device->SetRenderState ( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
  g_device->SetRenderState ( D3DRS_SRCBLENDALPHA, D3DBLEND_INVDESTALPHA );
  g_device->SetRenderState ( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
  g_device->SetRenderState ( D3DRS_DESTBLENDALPHA, D3DBLEND_ONE );
  g_device->SetRenderState ( D3DRS_ALPHATESTENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_SEPARATEALPHABLENDENABLE, TRUE );
  g_device->SetRenderState ( D3DRS_ALPHAREF, 0x08 );
  g_device->SetRenderState ( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
  g_device->SetRenderState ( D3DRS_LIGHTING, FALSE );
  g_device->SetRenderState ( D3DRS_FILLMODE, D3DFILL_SOLID );
  g_device->SetRenderState ( D3DRS_CULLMODE, D3DCULL_NONE );
  g_device->SetRenderState ( D3DRS_SCISSORTESTENABLE, TRUE );
  g_device->SetRenderState ( D3DRS_ZWRITEENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_STENCILENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_CLIPPING, TRUE );
  g_device->SetRenderState ( D3DRS_CLIPPLANEENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_VERTEXBLEND, D3DVBF_DISABLE );
  g_device->SetRenderState ( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_FOGENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_SRGBWRITEENABLE, FALSE );
  g_device->SetRenderState ( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );
  g_device->SetRenderState ( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
  g_device->SetRenderState ( D3DRS_ANTIALIASEDLINEENABLE, FALSE );

  g_device->SetTextureStageState ( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
  g_device->SetTextureStageState ( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
  g_device->SetTextureStageState ( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
  g_device->SetTextureStageState ( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
  g_device->SetTextureStageState ( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
  g_device->SetTextureStageState ( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
  g_device->SetTextureStageState ( 0, D3DTSS_TEXCOORDINDEX, 0 );
  g_device->SetTextureStageState ( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
  g_device->SetTextureStageState ( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
  g_device->SetTextureStageState ( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

  g_device->SetSamplerState ( 0ul, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
  g_device->SetSamplerState ( 0ul, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
  g_device->SetSamplerState ( 0ul, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );
  g_device->SetSamplerState ( 0, D3DSAMP_MINFILTER, D3DTEXF_PYRAMIDALQUAD );
  g_device->SetSamplerState ( 0, D3DSAMP_MAGFILTER, D3DTEXF_PYRAMIDALQUAD );
  g_device->SetSamplerState ( 0, D3DSAMP_MIPFILTER, D3DTEXF_PYRAMIDALQUAD );

  g_device->SetVertexShader ( nullptr );
  g_device->SetPixelShader ( nullptr );

  while ( !bail )
  {
    while ( PeekMessage ( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
    {
      TranslateMessage ( &msg );
      DispatchMessage ( &msg );
      if ( msg.message == WM_QUIT )
        bail = true;
    }

    if ( bail )
      break;

    daisy::color_t col { 0, 0, 0, 255 };
    g_device->Clear ( 0, nullptr, D3DCLEAR_TARGET, col.bgra, 1.0f, 0 );
    g_device->BeginScene ( );

    queue.push_filled_triangle ( { 0, 720 }, { 1280 / 2, 0 }, { 1280, 720 }, { 255, 0, 0, 72 }, { 0, 255, 0, 72 }, { 0, 0, 255, 72 } );
    queue.push_text< std::wstring_view > ( wrap, { 10, 10 }, L"this is a test for wide text\n朋友你好", { 255, 255, 255, 192 } );
    queue.push_text< std::string_view > ( wrap, { 1280 / 2, 800 / 2 }, "this ascii text is at the center of the window", { 255, 255, 255, 255 }, daisy::TEXT_ALIGNX_CENTER | daisy::TEXT_ALIGNY_CENTER );
    queue.push_text< std::string_view > ( verd, { 1280 / 2, 30 }, "this ascii text is at the top of the window! oh my science!", { 255, 255, 255, 255 }, daisy::TEXT_ALIGNX_CENTER | daisy::TEXT_ALIGNY_CENTER );
    queue.push_text< std::string_view > ( cmd, { 1280 / 2, 760 }, "this ascii text is at the bottom of the window! oh my science!", { 255, 255, 255, 255 }, daisy::TEXT_ALIGNX_CENTER | daisy::TEXT_ALIGNY_BOTTOM );
    queue.flush ( );
    queue.clear ( );

    g_device->EndScene ( );
    g_device->Present ( nullptr, nullptr, nullptr, nullptr );
  }

  // bail
  DestroyWindow ( hwnd );
  UnregisterClass ( wc.lpszClassName, wc.hInstance );

  return EXIT_SUCCESS;
}
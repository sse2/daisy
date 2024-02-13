// warning! no d3d9/winapi error handling in this **EXAMPLE**! this is meant to be a quick & dirty way to test stuff, with no guarantees.
// also of note; device resets aren't handled in the example yet. i'll get to it eventually, but it's not a priority right now. 
// if you want to learn about device reset handling with daisy, read the usage walkthrough in the README.md

// shut msvc up
#define _CRT_SECURE_NO_WARNINGS 1

// include daisy
#include "../daisy.hh"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <thread>
#include <optional>
#include <fstream>

#include <d3d9.h>
#include <tchar.h>

#undef _CRT_SECURE_NO_WARNINGS

#pragma comment( lib, "d3d9.lib" )

// parses image and returns A8R8G8B8 texture
std::vector< uint8_t > create_texture_from_image ( const std::string_view path, int &w, int &h )
{
  std::vector< uint8_t > ret { };
  int chan;

  stbi_set_flip_vertically_on_load ( false );
  unsigned char *tex_data = stbi_load_from_file ( fopen ( path.data ( ), "rb" ), &w, &h, &chan, 4 );
  if ( !tex_data )
    return ret;

  ret.resize ( w * h * 4 );
  memcpy ( ret.data ( ), tex_data, w * h * 4 );
  stbi_image_free ( tex_data );

  return ret;
}

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

  // initialize the library with the d3d9 device pointer
  daisy::daisy_initialize ( g_device );

  ShowWindow ( hwnd, SW_SHOWDEFAULT );
  UpdateWindow ( hwnd );

  // main window loop
  MSG msg;
  bool bail = false;

  // create a normal queue
  daisy::c_renderqueue queue;
  if ( !queue.create ( 32, 64 ) )
    return EXIT_FAILURE;

  // create a double buffer queue (you can fill this from a non-rendering thread safely)
  daisy::c_doublebuffer_queue double_buffer_queue;
  if ( !double_buffer_queue.create ( ) )
    return EXIT_FAILURE;

  // create font objects
  daisy::c_fontwrapper font_gothic;
  if ( !font_gothic.create ( "MS UI Gothic", 10, CLEARTYPE_NATURAL_QUALITY, daisy::FONT_DEFAULT ) )
    return EXIT_FAILURE;

  daisy::c_fontwrapper font_logo;
  if ( !font_logo.create ( "Arial Italic", 26, CLEARTYPE_NATURAL_QUALITY, daisy::FONT_DEFAULT ) )
    return EXIT_FAILURE;

  // create texture atlas object
  daisy::c_texatlas atlas;
  if ( !atlas.create ( { 2048.f, 2048.f } ) )
    return EXIT_FAILURE;

  // append an image to atlas 
  {
    int w, h;
    auto daisy_tex = create_texture_from_image ( "daisy.jpg", w, h );
    if ( !daisy_tex.empty ( ) )
    {
      atlas.append ( 1, { static_cast< float > ( w ), static_cast< float > ( h ) }, daisy_tex.data ( ), daisy_tex.size ( ) );
    }
  }

  // clang-format off
  // kick off a thread that fills our double buffer queue and swaps every second.
  std::thread {
  [ & ] ( ) {
    bool t = false;

    while ( true )
    {
      double_buffer_queue.queue ( )->clear ( );
      double_buffer_queue.queue ( )->push_text< std::wstring_view > ( font_gothic, { 10, 30 }, L"this draw list is updated from another thread once per second!", ( t ? daisy::color_t { 255, 0, 0, 192 } : daisy::color_t { 0, 255, 0, 192 } ) );
      double_buffer_queue.swap ( );

      t = !t;

      std::this_thread::sleep_for ( std::chrono::seconds ( 1 ) );
    }
  } }.detach ( );
  // clang-format on

  // prepare d3d9 state for rendering
  // if daisy is the only thing you plan on rendering with, you can only do this once *safely*
  // if you're implementing daisy alongside in a codebase that changes device state
  // you probably want to call this each frame before flushing render queues
  daisy::daisy_prepare ( );

  // render loop
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

    // util hacky timing stuff
    static float frametime = 0.f, realtime = 0.f;
    static std::clock_t current_time = 0, old_time = 0;

    const auto clock_to_seconds = [ & ] ( std::clock_t ticks ) { return ( ticks / static_cast< float > ( CLOCKS_PER_SEC ) ); };

    old_time = current_time;
    current_time = std::clock ( );
    frametime = clock_to_seconds ( current_time - old_time );
    realtime += frametime;

    daisy::color_t col { 0, 0, 0, 255 };
    g_device->Clear ( 0, nullptr, D3DCLEAR_TARGET, col.bgra, 1.0f, 0 );
    g_device->BeginScene ( );

    // push a filled triangle
    queue.push_filled_triangle ( { 0, 720 }, { 1280 / 2, 0 }, { 1280, 720 }, { 255, 0, 0, 72 }, { 0, 255, 0, 72 }, { 0, 0, 255, 72 } );

    // push a filled rectangle
    queue.push_filled_rectangle ( { 0, 720 }, { 1280 / 2, 0 }, { 255, 255, 255, 36 } );

    // push a filled gradient rectangle
    queue.push_gradient_rectangle ( { 0, 0 }, { 1280, 800 }, { 255, 255, 255, 36 }, { 255, 0, 0, 36 }, { 0, 255, 0, 36 }, { 0, 0, 255, 36 } );

    // push some wide text
    queue.push_text< std::wstring_view > ( font_gothic, { 10, 10 }, L"this is a test for wide text! 朋友你好!\nthis is a test for wide text! 朋友你好!\nthis is a test for wide text! 朋友你好!", { 255, 255, 255, 192 } );

    // push some ascii text
    queue.push_text< std::string_view > ( font_logo, { 1280 / 2, 800 / 2 + 50 * sinf ( realtime * 3.f ) }, "this ascii text is at the center of the window. also, it has a different font!", daisy::color_t::from_hsv ( fmodf ( realtime * 30.f, 360.f ), 0.6f, 1.f ), daisy::TEXT_ALIGNX_CENTER | daisy::TEXT_ALIGNY_CENTER );

    // push the daisy logo from the texture atlas
    auto daisy_image_coords = atlas.coords ( 1 );
    queue.push_filled_rectangle ( { 500 + 20 * sinf ( realtime * 3.f ), 100 + 20 * sinf ( realtime * 3.f ) }, { 448 + 20 * sinf ( realtime * 3.f ), 93 + 4 * sinf ( realtime * 3.f ) }, { 255, 255, 255 }, atlas.texture_handle ( ), { daisy_image_coords[ 0 ], daisy_image_coords[ 1 ] }, { daisy_image_coords[ 2 ], daisy_image_coords[ 3 ] } );

    // flushing of all render queues should happen here
    queue.flush ( );
    double_buffer_queue.flush ( );

    // clearing is necessary if your queue has dynamic data
    queue.clear ( );

    g_device->EndScene ( );
    g_device->Present ( nullptr, nullptr, nullptr, nullptr );
  }

  daisy::daisy_shutdown ( );

  // bail
  DestroyWindow ( hwnd );
  UnregisterClass ( wc.lpszClassName, wc.hInstance );

  return EXIT_SUCCESS;
}
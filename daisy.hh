#ifndef _SSE2_DAISY_INCLUDE_GUARD
#define _SSE2_DAISY_INCLUDE_GUARD

// stl includes
#include <unordered_map> // std::unordered_map
#include <string_view> // std::string_view
#include <vector> // std::vector
#include <array> // std::array
#include <cstdint> // uint/int types
 
// d3d9
#include <d3d9.h>

namespace daisy
{
  // our color struct
  struct color_t
  {
    union {
      struct
      {
        uint8_t b, g, r, a;
      } chan;

      uint32_t bgra;
    };

    constexpr color_t ( uint8_t _r = 255, uint8_t _g = 255, uint8_t _b = 255, uint8_t _a = 255 ) noexcept
        : chan ( { _b, _g, _r, _a } )
    {
    }
  };

  // our vertex struct
  struct daisy_vtx_t
  {
    float m_pos[ 4 ]; // x,y,z,rhw
    D3DCOLOR m_col;
    float m_uv[ 2 ];
  };

  // buffer
  struct renderbuffer_t
  {
    void *m_data;
    uint32_t m_capacity, m_size;
  };

  // kind of calls submitted
  enum class daisy_call_kind : uint8_t
  {
    CALL_TRI = 0,
    CALL_VTXSHADER,
    CALL_PIXSHADER,
    CALL_SCISSOR
  };

  // used for text calls
  enum daisy_text_align : uint16_t
  {
    TEXT_ALIGN_DEFAULT = 0,
    TEXT_ALIGNX_LEFT = 1 << 0,
    TEXT_ALIGNX_CENTER = 1 << 1,
    TEXT_ALIGNX_RIGHT = 1 << 2,
    TEXT_ALIGNY_TOP = 1 << 3,
    TEXT_ALIGNY_CENTER = 1 << 4,
    TEXT_ALIGNY_BOTTOM = 1 << 5,
  };

  // flags for font ctor
  enum daisy_font_flags : uint8_t
  {
    FONT_DEFAULT = 0,
    FONT_BOLD = 1 << 0,
    FONT_ITALIC = 1 << 1
  };

  using uv_t = std::array< float, 4 >;

  struct point_t
  {
    float x, y;
  };

  struct daisy_drawcall_t
  {
    daisy_call_kind m_kind;

    union {
      // for CALL_TRI
      struct
      {
        IDirect3DTexture9 *m_texture_handle;
        uint32_t m_primitives, m_vertices, m_indices;
      } m_tri;

      // for shader pop/push calls
      struct
      {
        void *m_shader_handle;
      } m_shader;

      // for scissor pop/push calls
      struct
      {
        point_t m_position, m_size;
      } m_scissor;
    };
  };

  // this is the only global object. we need this because the alternative would be passing around the pointer to each texture atlas and font wrapper instance
  // we don't really *need* it but it makes the code more readable
  struct daisy_t
  {
    static inline IDirect3DDevice9 *s_device = nullptr;
  };

  // our font wrapper class
  class c_daisy_fontwrapper
  {
  private:
    // members
    std::unordered_map< wchar_t, uv_t > m_coords;
    std::string_view m_family;
    IDirect3DTexture9 *m_texture_handle;
    float m_scale;
    uint32_t m_width, m_height, m_spacing, m_size, m_quality;
    uint8_t m_flags;

  private:
    // methods
    /// <summary>
    /// actually creates font instance and atlas
    /// </summary>
    /// <returns>true on succesful font creation, false otherwise</returns>
    bool create_ex ( ) noexcept
    {
      if ( !daisy_t::s_device )
        return false;

      HDC gdi_ctx = nullptr;
      HGDIOBJ gdi_font = nullptr, prev_gdi_font = nullptr, prev_bitmap = nullptr;
      HBITMAP bitmap = nullptr;

      // function could be possibly called by reset handler
      if ( this->m_texture_handle )
        this->m_texture_handle->Release ( );

      // create GDI context
      gdi_ctx = CreateCompatibleDC ( nullptr );
      SetMapMode ( gdi_ctx, MM_TEXT );

      // create GDI font
      this->create_gdi_font ( gdi_ctx, &gdi_font );

      prev_gdi_font = SelectObject ( gdi_ctx, gdi_font );

      // set default atlas size
      this->m_width = this->m_height = 128;

      // ensure our atlas is big enough
      while ( this->paint_or_measure_alphabet ( gdi_ctx, true ) == 2 )
      {
        this->m_width *= 2;
        this->m_height *= 2;
      }

      D3DCAPS9 caps;
      daisy_t::s_device->GetDeviceCaps ( &caps );

      // ensure our atlas isn't above max texture cap
      // @todo: split textures
      if ( this->m_width > static_cast< uint32_t > ( caps.MaxTextureWidth ) )
      {
        this->m_scale = static_cast< float > ( caps.MaxTextureWidth ) / this->m_width;
        this->m_width = this->m_height = caps.MaxTextureWidth;

        bool first_iteration = true;

        do
        {
          if ( !first_iteration )
            this->m_scale *= 0.9f;

          DeleteObject ( SelectObject ( gdi_ctx, prev_gdi_font ) );

          this->create_gdi_font ( gdi_ctx, &gdi_font );

          prev_gdi_font = SelectObject ( gdi_ctx, gdi_font );

          first_iteration = false;
        } while ( this->paint_or_measure_alphabet ( gdi_ctx, true ) == 2 );
      }

      // create dx9 tex
      HRESULT res = daisy_t::s_device->CreateTexture ( this->m_width, this->m_height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A4R4G4B4, D3DPOOL_DEFAULT, &this->m_texture_handle, nullptr );
      if ( res != D3D_OK || !this->m_texture_handle )
        return false;

      DWORD *bitmap_bits = nullptr;

      BITMAPINFO bitmap_ctx { };
      bitmap_ctx.bmiHeader.biSize = sizeof ( BITMAPINFOHEADER );
      bitmap_ctx.bmiHeader.biWidth = this->m_width;
      bitmap_ctx.bmiHeader.biHeight = -this->m_height;
      bitmap_ctx.bmiHeader.biPlanes = 1;
      bitmap_ctx.bmiHeader.biCompression = BI_RGB;
      bitmap_ctx.bmiHeader.biBitCount = 32;

      bitmap = CreateDIBSection ( gdi_ctx, &bitmap_ctx, DIB_RGB_COLORS, reinterpret_cast< void ** > ( &bitmap_bits ), nullptr, 0 );
      prev_bitmap = SelectObject ( gdi_ctx, bitmap );

      SetTextColor ( gdi_ctx, RGB ( 255, 255, 255 ) );
      SetBkColor ( gdi_ctx, 0x00000000 );
      SetTextAlign ( gdi_ctx, TA_TOP );

      // to note: paint_alphabet returns 0 on success
      if ( this->paint_or_measure_alphabet ( gdi_ctx, false ) )
        return false;

      D3DLOCKED_RECT locked_rect;
      if ( this->m_texture_handle->LockRect ( 0, &locked_rect, nullptr, 0 ) != D3D_OK )
        return false;

      uint8_t *dst_row = static_cast< uint8_t * > ( locked_rect.pBits );
      BYTE alpha;

      // write to texture
      for ( uint32_t y = 0; y < this->m_height; y++ )
      {
        uint16_t *dst = reinterpret_cast< uint16_t * > ( dst_row );
        for ( uint32_t x = 0; x < this->m_width; x++ )
        {
          alpha = ( ( bitmap_bits[ this->m_width * y + x ] & 0xff ) >> 4 );
          if ( alpha > 0 )
          {
            *dst++ = ( ( alpha << 12 ) | 0x0fff );
          }
          else
          {
            *dst++ = 0x0000;
          }
        }
        dst_row += locked_rect.Pitch;
      }

      if ( this->m_texture_handle->UnlockRect ( 0 ) != D3D_OK )
        return false;

      // clean up
      SelectObject ( gdi_ctx, prev_bitmap );
      SelectObject ( gdi_ctx, prev_gdi_font );
      DeleteObject ( bitmap );
      DeleteObject ( gdi_font );
      DeleteDC ( gdi_ctx );

      return true;
    }

    /// <summary>
    /// creates GDI font
    /// </summary>
    /// <param name="context">GDI context</param>
    /// <param name="gdi_font">font GDI object</param>
    void create_gdi_font ( HDC context, HGDIOBJ *gdi_font ) noexcept
    {
      *gdi_font = CreateFontA ( -MulDiv ( this->m_size, static_cast< int > ( GetDeviceCaps ( context, LOGPIXELSY ) * this->m_scale ), 72 ),
                                0, 0, 0, ( this->m_flags & daisy_font_flags::FONT_BOLD ) ? FW_BOLD : FW_NORMAL, ( this->m_flags & daisy_font_flags::FONT_ITALIC ) ? TRUE : FALSE,
                                FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, this->m_quality, VARIABLE_PITCH, this->m_family.data ( ) );
    }

    /// <summary>
    /// measures or paints alphabet
    /// </summary>
    /// <param name="context">GDI context</param>
    /// <param name="measure">if we should only measure, not paint alphabet</param>
    /// <returns>0 on success, 1 on GDI error, 2 if texture atlas can't contain our glyphs</returns>
    int paint_or_measure_alphabet ( HDC context, bool measure = false ) noexcept
    {
      SIZE size;
      wchar_t chr[] = L"x\0\0";

      if ( !GetTextExtentPoint32W ( context, chr, 1, &size ) )
        return 1;

      // thanks dex for help with this
      const auto unicode_ranges_size = GetFontUnicodeRanges ( context, nullptr );
      if ( !unicode_ranges_size )
        return 1;

      // @todo; we're in c++ let's not use malloc... clogs up code
      GLYPHSET *glyph_sets = ( GLYPHSET * ) malloc ( unicode_ranges_size );
      if ( !glyph_sets )
        return 1;

      if ( !GetFontUnicodeRanges ( context, glyph_sets ) )
      {
        free ( glyph_sets );
        return 1;
      }

      this->m_spacing = static_cast< uint32_t > ( ceil ( size.cy * 0.3f ) );

      uint32_t x = this->m_spacing;
      uint32_t y = 0;

      // iterate glyph ranges
      for ( uint32_t r = 0; r < glyph_sets->cRanges; ++r )
      {
        // iterate glyphs
        for ( auto chr = glyph_sets->ranges[ r ].wcLow; chr < ( glyph_sets->ranges[ r ].wcLow + glyph_sets->ranges[ r ].cGlyphs ); ++chr )
        {
          // get metrics of the current character
          if ( !GetTextExtentPoint32W ( context, &chr, 1, &size ) )
            continue;

          if ( x + size.cx + this->m_spacing > this->m_width )
          {
            x = this->m_spacing;
            y += size.cy + 1;
          }

          // ran out of space in texture
          if ( y + size.cy > this->m_height )
          {
            free ( glyph_sets );
            return 2;
          }

          if ( !measure )
          {
            if ( !ExtTextOutW ( context, x + 0, y + 0, ETO_OPAQUE, nullptr, &chr, 1, nullptr ) )
            {
              free ( glyph_sets );
              return 1;
            }

            this->m_coords[ static_cast< uint16_t > ( chr ) ][ 0 ] = ( static_cast< float > ( x + 0 - this->m_spacing ) ) / this->m_width;
            this->m_coords[ static_cast< uint16_t > ( chr ) ][ 1 ] = ( static_cast< float > ( y + 0 + 0 ) ) / this->m_height;
            this->m_coords[ static_cast< uint16_t > ( chr ) ][ 2 ] = ( static_cast< float > ( x + size.cx + this->m_spacing ) ) / this->m_width;
            this->m_coords[ static_cast< uint16_t > ( chr ) ][ 3 ] = ( static_cast< float > ( y + size.cy + 0 ) ) / this->m_height;
          }

          x += size.cx + ( 2 * this->m_spacing );
        }
      }

      free ( glyph_sets );

      return 0;
    }

  public:
    // inits everything with 0
    c_daisy_fontwrapper ( ) noexcept
        : m_family ( ), m_texture_handle ( nullptr ), m_scale ( 0.f ), m_width ( 0 ), m_height ( 0 ), m_spacing ( 0 ), m_size ( 0 ), m_flags ( 0 )
    {
    }

    // disallow copying
    c_daisy_fontwrapper ( const c_daisy_fontwrapper & ) = delete;
    c_daisy_fontwrapper &operator= ( const c_daisy_fontwrapper & ) = delete;

    /// <summary>
    /// creates font instance and atlas
    /// </summary>
    /// <param name="family">font family name, for example "Arial" (to note; fonts added by AddFontMemResourceEx also work)</param>
    /// <param name="height">font height</param>
    /// <param name="quality">font quality (NONANTIALIASED_QUALITY, CLEARTYPE_NATURAL_QUALITY etc.)</param>
    /// <param name="flags">font flags (see enum daisy_font_flags; FONT_DEFAULT, FONT_BOLD, FONT_ITALIC)</param>
    /// <returns>true on succesful font creation, false otherwise</returns>
    bool create ( const std::string_view family, uint32_t height, uint32_t quality, uint8_t flags ) noexcept
    {
      this->m_family = family;
      this->m_size = height;
      this->m_flags = flags;
      this->m_quality = quality;
      this->m_scale = 1.f;
      this->m_spacing = 0;

      return this->create_ex ( );
    }

    /// <summary>
    /// returns measured text extent in pixels
    /// </summary>
    /// <typeparam name="t">iteratable text container (only char and wchar_t accepted currently)</typeparam>
    /// <param name="text">text to measure</param>
    /// <returns>point_t containing width and height of measured text</returns>
    template < typename t = std::string_view >
    point_t text_extent ( t text ) noexcept
    {
      float row_width = 0.f;
      float row_height = ( this->m_coords[ static_cast< wchar_t > ( 32 ) ][ 3 ] - this->m_coords[ static_cast< wchar_t > ( 32 ) ][ 1 ] ) * this->m_height;
      float width = 0.f;
      float height = row_height;

      for ( const auto c : text )
      {
        if constexpr ( sizeof ( c ) == sizeof ( char ) )
        {
          if ( c == '\n' )
          {
            row_width = 0.f;
            height += row_height;
          }
        }
        else
        {
          if ( c == L'\n' )
          {
            row_width = 0.f;
            height += row_height;
          }
        }

        if ( c < ' ' )
          continue;

        float tx1 = this->m_coords[ static_cast< wchar_t > ( c ) ][ 0 ];
        float tx2 = this->m_coords[ static_cast< wchar_t > ( c ) ][ 2 ];

        row_width += ( tx2 - tx1 ) * this->m_width - 2.f * this->m_spacing;

        if ( row_width > width )
          width = row_width;
      }

      return { width, height };
    }

    /// <summary>
    /// called on device reset (pre/post)
    /// </summary>
    /// <param name="pre_reset">if this is called before device is reset</param>
    /// <returns>true on success, false otherwise</returns>
    bool reset ( bool pre_reset = false ) noexcept
    {
      if ( !pre_reset )
        return this->create_ex ( );
      else if ( this->m_texture_handle )
        this->m_texture_handle->Release ( );

      return true;
    }

    /// <summary>
    /// erases the font
    /// </summary>
    void erase ( ) noexcept
    {
      if ( this->m_texture_handle )
        this->m_texture_handle->Release ( );

      this->m_coords.clear ( );

      this->m_size = this->m_spacing = this->m_flags = 0;
      this->m_scale = 1.f;
      this->m_family = "";
    }

    // getters

    /// <summary>
    /// get UV coordinates of glyph
    /// </summary>
    /// <typeparam name="t">char, wchar_t</typeparam>
    /// <param name="glyph">character to get glyph UV coordinates for</param>
    /// <returns>UV coordinates of glyph</returns>
    template < typename t = char >
    const uv_t &coords ( t glyph ) const noexcept
    {
      if ( this->m_coords.find ( glyph ) != this->m_coords.end ( ) )
        return this->m_coords.at ( glyph );

      return uv_t { 0.f, 0.f, 0.f, 0.f };
    }

    /// <summary>
    /// get font spacing
    /// </summary>
    /// <returns>font spacing</returns>
    uint32_t spacing ( ) const noexcept
    {
      return this->m_spacing;
    }

    /// <summary>
    /// get font width
    /// </summary>
    /// <returns>font width</returns>
    uint32_t width ( ) const noexcept
    {
      return this->m_width;
    }

    /// <summary>
    /// get font height
    /// </summary>
    /// <returns>font height</returns>
    uint32_t height ( ) const noexcept
    {
      return this->m_height;
    }

    /// <summary>
    /// get font scale
    /// </summary>
    /// <returns>font scale</returns>
    float scale ( ) const noexcept
    {
      return this->m_scale;
    }

    /// <summary>
    /// get texture handle
    /// </summary>
    /// <returns>texture handle</returns>
    IDirect3DTexture9 *texture_handle ( ) const noexcept
    {
      return this->m_texture_handle;
    }

    // @todo; clean
  };

  class c_daisytexatlas
  {
  private:
    std::unordered_map< uint32_t, uv_t > m_coords;
    point_t m_cursor, m_dimensions;
    IDirect3DTexture9 *m_texture_handle;
    float m_max_height;

  public:
    c_daisytexatlas ( ) noexcept
        : m_cursor ( { 0.f, 0.f } ), m_dimensions ( { 0.f, 0.f } ), m_texture_handle ( nullptr ), m_max_height ( 0.f )
    {
    }

    // disable copying
    c_daisytexatlas ( const c_daisytexatlas & ) = delete;
    c_daisytexatlas &operator= ( const c_daisytexatlas & ) = delete;

    /// <summary>
    /// creates a texture atlas
    /// </summary>
    /// <param name="dimensions">texture atlas dimensions</param>
    /// <returns>true on success, false otherwise</returns>
    bool create ( const point_t &dimensions ) noexcept
    {
      if ( !daisy_t::s_device )
        return false;

      this->m_dimensions = dimensions;
      this->m_cursor = point_t { 0.f, 0.f };
      this->m_max_height = 0.f;

      if ( daisy_t::s_device->CreateTexture ( static_cast< UINT > ( dimensions.x ), static_cast< UINT > ( dimensions.y ), 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &this->m_texture_handle, nullptr ) != D3D_OK )
        return false;

      return true;
    }

    /// <summary>
    /// called on device reset (pre/post)
    /// </summary>
    /// <param name="pre_reset">if this is called before device is reset</param>
    /// <returns>true on success, false otherwise</returns>
    bool reset ( bool pre_reset = false ) noexcept
    {
      if ( !pre_reset )
        return this->create ( this->m_dimensions );
      else if ( this->m_texture_handle )
        this->m_texture_handle->Release ( );

      return true;
    }

    /// <summary>
    /// appends a texture to the texture atlas
    /// </summary>
    /// <param name="uuid">uuid of texture (can be whatever you want as long as it's unique per texture)</param>
    /// <param name="dimensions">dimensions of texture in pixels</param>
    /// <param name="tex_data">RGBA texture data</param>
    /// <param name="tex_size">size of texture data in bytes</param>
    /// <returns>true on success, false otherwise</returns>
    bool append ( const uint32_t uuid, const point_t &dimensions, uint8_t *tex_data, uint32_t tex_size )
    {
      // go down if not enough space left
      if ( this->m_cursor.x + dimensions.x > this->m_dimensions.x )
      {
        this->m_cursor.y += this->m_max_height;
        this->m_cursor.x = this->m_max_height = 0.f;
      }

      // not enough space left
      if ( this->m_cursor.y + dimensions.y > this->m_dimensions.y )
        return false;

      // ensure we set max height
      if ( this->m_max_height < dimensions.y )
        this->m_max_height = dimensions.y;

      // lock tex
      D3DLOCKED_RECT tex_locked_rect;

      if ( this->m_texture_handle->LockRect ( 0, &tex_locked_rect, NULL, 0 ) != D3D_OK )
        return false;

      // copy tex data over
      for ( int y = 0; y < dimensions.y; ++y )
      {
        for ( int x = 0; x < dimensions.x; ++x )
        {
          const uint8_t *source_pixel = tex_data + static_cast< uint32_t > ( dimensions.x ) * 4 * y + x * 4;

          // ensure the texture size matches what we expect
          if ( static_cast< uint32_t > ( dimensions.x ) * 4 * y + x * 4 > tex_size )
            return false;

          uint8_t *destination_pixel = static_cast< uint8_t * > ( tex_locked_rect.pBits ) + tex_locked_rect.Pitch * ( static_cast< uint32_t > ( this->m_cursor.y ) + y ) + ( static_cast< uint32_t > ( this->m_cursor.x ) + x ) * 4;

          destination_pixel[ 0 ] = source_pixel[ 2 ];
          destination_pixel[ 1 ] = source_pixel[ 1 ];
          destination_pixel[ 2 ] = source_pixel[ 0 ];
          destination_pixel[ 3 ] = source_pixel[ 3 ];
        }
      }

      this->m_texture_handle->UnlockRect ( 0 );

      // set uv mins/maxs
      auto start_uv = point_t { this->m_cursor.x / this->m_dimensions.x, this->m_cursor.y / this->m_dimensions.y };
      auto end_uv = point_t { start_uv.x + dimensions.x / this->m_dimensions.x, start_uv.y + dimensions.y / this->m_dimensions.y };
      this->m_coords[ uuid ] = uv_t { start_uv.x, start_uv.y, end_uv.x, end_uv.y };

      // move cursor over
      this->m_cursor.x += dimensions.x;

      return true;
    }

    /// <summary>
    /// get UV coordinates of texture
    /// </summary>
    /// <param name="glyph">character to get texture UV coordinates for</param>
    /// <returns>UV coordinates of texture</returns>
    const uv_t &coords ( uint32_t uuid ) const noexcept
    {
      return this->m_coords.at ( uuid );
    }

    /// <summary>
    /// get texture handle
    /// </summary>
    /// <returns>texture handle</returns>
    IDirect3DTexture9 *texture_handle ( ) const noexcept
    {
      return this->m_texture_handle;
    }
  };

  class c_renderqueue
  {
  private:
    IDirect3DVertexBuffer9 *m_vertex_buffer;
    IDirect3DIndexBuffer9 *m_index_buffer;

    renderbuffer_t m_vtxs, m_idxs;

    std::vector< daisy_drawcall_t > m_drawcalls;

    // update d3d9 sided vtx/idx buffers
    bool m_update;

  private:
    /// <summary>
    /// checks if call can be batched
    /// </summary>
    /// <param name="texture_handle">texture handle</param>
    /// <returns>0 if we can't batch this call, index offset on success</returns>
    uint32_t begin_batch ( IDirect3DTexture9 *texture_handle = nullptr )
    {
      uint32_t additional = 0;

      // attempt to batch drawcall
      if ( !this->m_drawcalls.empty ( ) )
      {
        auto &last_call = this->m_drawcalls.at ( this->m_drawcalls.size ( ) - 1 );
        if ( last_call.m_kind == daisy_call_kind::CALL_TRI && last_call.m_tri.m_texture_handle == texture_handle )
        {
          // we can batch this call
          additional = last_call.m_tri.m_vertices;
        }
      }

      return additional;
    }

    /// <summary>
    /// appends call to batch if possible
    /// </summary>
    /// <param name="additional_indices">return value of begin_batch() call</param>
    /// <param name="vertices">vertices in call</param>
    /// <param name="indices">indices in call</param>
    /// <param name="primitives">primitives in call</param>
    /// <param name="texture_handle">texutre handle</param>
    void end_batch ( uint32_t additional_indices, uint32_t vertices, uint32_t indices, uint32_t primitives, IDirect3DTexture9 *texture_handle = nullptr )
    {
      // call can't be batched
      if ( !additional_indices )
      {
        daisy_drawcall_t d { };
        d.m_kind = daisy_call_kind::CALL_TRI;
        d.m_tri.m_indices = indices;
        d.m_tri.m_vertices = vertices;
        d.m_tri.m_texture_handle = texture_handle;
        d.m_tri.m_primitives = primitives;

        this->m_drawcalls.push_back ( std::move ( d ) );
      }
      // call is batched
      else
      {
        this->m_drawcalls.at ( this->m_drawcalls.size ( ) - 1 ).m_tri.m_vertices += vertices;
        this->m_drawcalls.at ( this->m_drawcalls.size ( ) - 1 ).m_tri.m_indices += indices;
        this->m_drawcalls.at ( this->m_drawcalls.size ( ) - 1 ).m_tri.m_primitives += primitives;
      }

      // need to update gpu-side buffers
      this->m_update = true;

    }
  public:
    c_renderqueue ( ) noexcept
        : m_vertex_buffer ( nullptr ), m_index_buffer ( nullptr ), m_update ( true )
    {
    }

    // disallow copying
    c_renderqueue ( const c_renderqueue & ) = delete;
    c_renderqueue &operator= ( const c_renderqueue & ) = delete;

    /// <summary>
    /// initializes current queue by initializing vertex and index buffers
    /// </summary>
    /// <param name="mex_verts">max capacity of vertex buffer</param>
    /// <param name="max_indices">max capacity of index buffer</param>
    /// <returns>true on success, false otherwise</returns>
    bool create ( const uint32_t mex_verts = 32767, const uint32_t max_indices = 65535 ) noexcept
    {
      if ( !daisy_t::s_device )
        return false;

      // create gpu buffers
      if ( !this->m_vertex_buffer )
        if ( daisy_t::s_device->CreateVertexBuffer ( sizeof ( daisy_vtx_t ) * mex_verts, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 ), D3DPOOL_DEFAULT, &this->m_vertex_buffer, nullptr ) < 0 )
          return false;

      if ( !this->m_index_buffer )
        if ( daisy_t::s_device->CreateIndexBuffer ( sizeof ( uint16_t ) * max_indices, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &this->m_index_buffer, nullptr ) < 0 )
          return false;

      // create cpu side buffers
      this->m_vtxs.m_data = malloc ( sizeof ( daisy_vtx_t ) * mex_verts );
      this->m_vtxs.m_capacity = mex_verts;
      this->m_vtxs.m_size = 0;

      this->m_idxs.m_data = malloc ( sizeof ( uint16_t ) * max_indices );
      this->m_idxs.m_capacity = max_indices;
      this->m_idxs.m_size = 0;

      if ( !this->m_vtxs.m_data || !this->m_idxs.m_data )
        return false;

      return true;
    }

    /// <summary>
    /// wipes data from local buffers
    /// </summary>
    void clear ( ) noexcept
    {
      this->m_vtxs.m_size = 0;
      this->m_idxs.m_size = 0;

      if ( !this->m_drawcalls.empty ( ) )
        this->m_drawcalls.clear ( );
    }

    /// <summary>
    /// copies data from local buffer to d3d9 buffers
    /// </summary>
    void update ( ) noexcept
    {
      daisy_vtx_t *vert;
      uint16_t *indx;

      // lock buffers
      if ( this->m_vertex_buffer->Lock ( 0, static_cast< UINT > ( this->m_vtxs.m_size * sizeof ( daisy_vtx_t ) ), ( void ** ) &vert, D3DLOCK_DISCARD ) < 0 )
        return;

      if ( this->m_index_buffer->Lock ( 0, static_cast< UINT > ( this->m_idxs.m_size * sizeof ( uint16_t ) ), ( void ** ) &indx, D3DLOCK_DISCARD ) < 0 )
      {
        this->m_vertex_buffer->Unlock ( );
        return;
      }

      // we can just memcpy these
      memcpy ( vert, this->m_vtxs.m_data, sizeof ( daisy_vtx_t ) * this->m_vtxs.m_size );
      memcpy ( indx, this->m_idxs.m_data, sizeof ( uint16_t ) * this->m_idxs.m_size );

      // unlock and ret
      this->m_vertex_buffer->Unlock ( );
      this->m_index_buffer->Unlock ( );

      // we no longer need to update
      this->m_update = false;
    }

    /// <summary>
    /// flushes vertices and indices to d3d9 buffer if an update is required and actually draws primitives
    /// </summary>
    void flush ( ) noexcept
    {
      if ( this->m_drawcalls.empty ( ) )
        return;

      // modify buffers only if required
      if ( this->m_update )
        this->update ( );

      daisy_t::s_device->SetStreamSource ( 0, this->m_vertex_buffer, 0, sizeof ( daisy_vtx_t ) );
      daisy_t::s_device->SetIndices ( this->m_index_buffer );
      daisy_t::s_device->SetFVF ( ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 ) );

      uint32_t vertex_idx { 0 }, index_idx { 0 };

      // render commands
      for ( const auto &cmd : this->m_drawcalls )
      {
        // @todo: support shaders as well
        switch ( cmd.m_kind )
        {
        case daisy_call_kind::CALL_TRI:
          daisy_t::s_device->SetTexture ( 0, cmd.m_tri.m_texture_handle );
          daisy_t::s_device->DrawIndexedPrimitive ( D3DPT_TRIANGLELIST, vertex_idx, 0, cmd.m_tri.m_vertices, index_idx, cmd.m_tri.m_primitives );

          vertex_idx += cmd.m_tri.m_vertices;
          index_idx += cmd.m_tri.m_indices;
          break;
        case daisy_call_kind::CALL_VTXSHADER:
          daisy_t::s_device->SetVertexShader ( reinterpret_cast< IDirect3DVertexShader9 * > ( cmd.m_shader.m_shader_handle ) );
          break;
        case daisy_call_kind::CALL_PIXSHADER:
          daisy_t::s_device->SetPixelShader ( reinterpret_cast< IDirect3DPixelShader9 * > ( cmd.m_shader.m_shader_handle ) );
          break;
        case daisy_call_kind::CALL_SCISSOR: {
          RECT r { static_cast< LONG > ( cmd.m_scissor.m_position.x ), static_cast< LONG > ( cmd.m_scissor.m_position.y ),
                   static_cast< LONG > ( cmd.m_scissor.m_position.x + cmd.m_scissor.m_size.x ),
                   static_cast< LONG > ( cmd.m_scissor.m_position.y + cmd.m_scissor.m_size.y ) };

          daisy_t::s_device->SetScissorRect ( &r );
        }
        break;
        }
      }
    }

    /// <summary>
    /// clips viewport to a certain rectangle
    /// </summary>
    /// <param name="position">rectangle position</param>
    /// <param name="size">rectangle size</param>
    void push_scissor ( point_t &position, point_t &size ) noexcept
    {
      daisy_drawcall_t d { };
      d.m_kind = daisy_call_kind::CALL_SCISSOR;
      d.m_scissor.m_position = position;
      d.m_scissor.m_size = size;

      this->m_drawcalls.push_back ( std::move ( d ) );
    }

    /// <summary>
    /// push gradient rectangle to drawlist
    /// </summary>
    /// <param name="position">rectangle position</param>
    /// <param name="size">rectangle size</param>
    /// <param name="c1">top left rectangle color</param>
    /// <param name="c2">top right rectangle color</param>
    /// <param name="c3">bottom left rectangle color</param>
    /// <param name="c4">bottom right rectangle color</param>
    /// <param name="texture_handle">texture handle (by default nullptr, means no texture is applied)</param>
    /// <param name="uv_mins">uv mins of rectangle in texture (by default {0, 0})</param>
    /// <param name="uv_maxs">uv maxs of rectangle in texture (by default {1, 1})</param>
    void push_gradient_rectangle ( const point_t &position, const point_t &size, const color_t c1, const color_t c2, const color_t c3, const color_t c4, IDirect3DTexture9 *texture_handle = nullptr, const point_t &uv_mins = { 0.f, 0.f }, const point_t &uv_maxs = { 1.f, 1.f } ) noexcept
    {
      uint32_t additional_indices = this->begin_batch ( texture_handle );

      daisy_vtx_t vtx[] = { daisy_vtx_t { { floorf ( position.x ), floorf ( position.y ), 0.0f, 1.f }, c1.bgra, { uv_mins.x, uv_mins.y } },                   // top-left
                            daisy_vtx_t { { floorf ( position.x + size.x ), floorf ( position.y ), 0.0f, 1.f }, c2.bgra, { uv_maxs.x, uv_mins.y } },          // top-right
                            daisy_vtx_t { { floorf ( position.x + size.x ), floorf ( position.y + size.y ), 0.0f, 1.f }, c4.bgra, { uv_maxs.x, uv_maxs.y } }, // bottom-right
                            daisy_vtx_t { { floorf ( position.x ), floorf ( position.y + size.y ), 0.0f, 1.f }, c3.bgra, { uv_mins.x, uv_maxs.y } } };        // bottom-left

      uint16_t idxs[] = { static_cast< uint16_t > ( additional_indices ),
                          static_cast< uint16_t > ( additional_indices + 1 ),
                          static_cast< uint16_t > ( additional_indices + 3 ),
                          static_cast< uint16_t > ( additional_indices + 3 ),
                          static_cast< uint16_t > ( additional_indices + 2 ),
                          static_cast< uint16_t > ( additional_indices + 1 ) };

      memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_vtxs.m_data ) + ( sizeof ( daisy_vtx_t ) * this->m_vtxs.m_size ) ), &vtx, sizeof ( daisy_vtx_t ) * 4 );
      memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_idxs.m_data ) + ( sizeof ( uint16_t ) * this->m_idxs.m_size ) ), &idxs, sizeof ( uint16_t ) * 6 );

      this->m_vtxs.m_size += 4;
      this->m_idxs.m_size += 6;

      this->end_batch ( additional_indices, 4, 6, 2, texture_handle );
    }

    /// <summary>
    /// push filled rectangle to drawlist
    /// </summary>
    /// <param name="position">rectangle position</param>
    /// <param name="size">rectangle size</param>
    /// <param name="col">rectangle color</param>
    /// <param name="texture_handle">texture handle (by default nullptr, means no texture is applied)</param>
    /// <param name="uv_mins">uv mins of rectangle in texture (by default {0, 0})</param>
    /// <param name="uv_maxs">uv maxs of rectangle in texture (by default {1, 1})</param>
    void push_filled_rectangle ( const point_t &position, const point_t &size, const color_t col, IDirect3DTexture9 *texture_handle = nullptr, const point_t &uv_mins = { 0.f, 0.f }, const point_t &uv_maxs = { 1.f, 1.f } ) noexcept
    {
      this->push_gradient_rectangle ( position, size, col, col, col, col, texture_handle, uv_mins, uv_maxs );
    }

    /// <summary>
    /// push filled triangle to drawlist
    /// </summary>
    /// <param name="p1">point 1 of triangle</param>
    /// <param name="p2">point 2 of triangle</param>
    /// <param name="p3">point 3 of triangle</param>
    /// <param name="c1">color 1 of triangle</param>
    /// <param name="c2">color 2 of triangle</param>
    /// <param name="c3">color 3 of triangle</param>
    /// <param name="texture_handle">texture handle (by default nullptr, means no texture is applied)</param>
    /// <param name="uv1">uv bounds for the 1st point</param>
    /// <param name="uv2">uv bounds for the 2nd point</param>
    /// <param name="uv3">uv bounds for the 3rd point</param>
    void push_filled_triangle ( const point_t &p1, const point_t &p2, const point_t &p3, const color_t c1, const color_t c2, const color_t c3, IDirect3DTexture9 *texture_handle = nullptr, const point_t &uv1 = { 0.f, 0.f }, const point_t &uv2 = { 0.f, 0.f }, const point_t &uv3 = { 0.f, 0.f } ) noexcept
    {
      uint32_t additional_indices = this->begin_batch ( texture_handle );

      daisy_vtx_t vtx[] = { daisy_vtx_t { { floorf ( p1.x ), floorf ( p1.y ), 0.0f, 1.f }, c1.bgra, { uv1.x, uv1.y } },
                            daisy_vtx_t { { floorf ( p2.x ), floorf ( p2.y ), 0.0f, 1.f }, c2.bgra, { uv2.x, uv2.y } },
                            daisy_vtx_t { { floorf ( p3.x ), floorf ( p3.y ), 0.0f, 1.f }, c3.bgra, { uv3.x, uv3.y } } };

      uint16_t idxs[] = { static_cast< uint16_t > ( additional_indices ),
                          static_cast< uint16_t > ( additional_indices + 1 ),
                          static_cast< uint16_t > ( additional_indices + 2 ) };

      memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_vtxs.m_data ) + ( sizeof ( daisy_vtx_t ) * this->m_vtxs.m_size ) ), &vtx, sizeof ( daisy_vtx_t ) * 3 );
      memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_idxs.m_data ) + ( sizeof ( uint16_t ) * this->m_idxs.m_size ) ), &idxs, sizeof ( uint16_t ) * 3 );

      this->m_vtxs.m_size += 3;
      this->m_idxs.m_size += 3;

      this->end_batch ( additional_indices, 3, 3, 1, texture_handle );
    }

    /// <summary>
    /// push line to drawlist
    /// </summary>
    /// <param name="p1">point 1 of line</param>
    /// <param name="p2">point 2 of line</param>
    /// <param name="col">color of line</param>
    /// <param name="width">width of line</param>
    void push_line ( const point_t &p1, const point_t &p2, const color_t &col, const float width = 1.f ) noexcept
    {
      uint32_t additional_indices = this->begin_batch ( nullptr );

      // shoutout 8th grade math
      point_t delta = { p2.x - p1.x , p2.y - p1.y };
      float length = sqrtf ( delta.x * delta.x + delta.y * delta.y ) + FLT_EPSILON;

      float scale = width / ( 2.f * length );
      point_t radius = { -scale * delta.y, scale * delta.x };

      daisy_vtx_t vtx[] = { daisy_vtx_t { { p1.x - radius.x, p1.y - radius.y, 0.0f, 1.f }, col.bgra, { 0.f, 0.f } },
                            daisy_vtx_t { { p1.x + radius.x, p1.y + radius.y, 0.0f, 1.f }, col.bgra, { 1.f, 0.f } },
                            daisy_vtx_t { { p2.x - radius.x, p2.y - radius.y, 0.0f, 1.f }, col.bgra, { 1.f, 1.f } },
                            daisy_vtx_t { { p2.x + radius.x, p2.y + radius.y, 0.0f, 1.f }, col.bgra, { 0.f, 1.f } } };

      uint16_t idxs[] = { static_cast< uint16_t > ( additional_indices ),
                          static_cast< uint16_t > ( additional_indices + 1 ),
                          static_cast< uint16_t > ( additional_indices + 2 ),
                          static_cast< uint16_t > ( additional_indices + 2 ),
                          static_cast< uint16_t > ( additional_indices + 3 ),
                          static_cast< uint16_t > ( additional_indices + 1 ) };

      memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_vtxs.m_data ) + ( sizeof ( daisy_vtx_t ) * this->m_vtxs.m_size ) ), &vtx, sizeof ( daisy_vtx_t ) * 4 );
      memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_idxs.m_data ) + ( sizeof ( uint16_t ) * this->m_idxs.m_size ) ), &idxs, sizeof ( uint16_t ) * 6 );

      this->m_vtxs.m_size += 4;
      this->m_idxs.m_size += 6;

      this->end_batch ( additional_indices, 4, 6, 2, nullptr );
    }

    /// <summary>
    /// push a string with a given font to drawlist
    /// </summary>
    /// <typeparam name="t">iteratable text container (only char and wchar_t accepted currently)</typeparam>
    /// <param name="font">initialized c_daisy_fontwrapper instance</param>
    /// <param name="position">position of text</param>
    /// <param name="text">text to draw</param>
    /// <param name="color">color of text to draw</param>
    /// <param name="alignment">alignment of text to draw</param>
    template<typename t = std::string_view>
    void push_text ( c_daisy_fontwrapper &font, const point_t &position, const t text, const color_t &color, uint16_t alignment = TEXT_ALIGN_DEFAULT ) noexcept
    {
      uint32_t additional_indices = this->begin_batch ( font.texture_handle ( ) );
      uint32_t cont_vertices = 0, cont_indices = 0, cont_primitives = 0;

      point_t corrected_position { position };

      if ( alignment != TEXT_ALIGN_DEFAULT )
      {
        const auto size = font.text_extent ( text );

        if ( alignment & TEXT_ALIGNX_CENTER )
          corrected_position.x -= floorf ( 0.5f * size.x );
        else if ( alignment & TEXT_ALIGNX_RIGHT )
          corrected_position.x -= floorf ( size.x );

        if ( alignment & TEXT_ALIGNY_CENTER )
          corrected_position.y -= floorf ( 0.5f * size.y );
        else if ( alignment & TEXT_ALIGNY_BOTTOM )
          corrected_position.y -= floorf ( size.y );
      }

      corrected_position.x -= font.spacing ( );

      float start_x = corrected_position.x;
      auto space_coords = font.coords ( 'A' );

      for ( const auto c : text )
      {
        bool is_space = false;

        if constexpr ( sizeof ( c ) == sizeof ( char ) )
        {
          if ( c == '\n' )
          {
            corrected_position.x = start_x;
            corrected_position.y += ( space_coords[ 3 ] - space_coords[ 1 ] ) * font.height ( );

            continue;
          }

          if ( c < ' ' )
            continue;

          is_space = c == ' ';
        }
        else
        {
          if ( c == L'\n' )
          {
            corrected_position.x = start_x;
            corrected_position.y += ( space_coords[ 3 ] - space_coords[ 1 ] ) * font.height ( );

            continue;
          }

          if ( c < L' ' )
            continue;

          is_space = c == L' ';
        }

        auto coords = font.coords ( c );

        float tx1 = coords[ 0 ];
        float ty1 = coords[ 1 ];
        float tx2 = coords[ 2 ];
        float ty2 = coords[ 3 ];

        float w = ( tx2 - tx1 ) * font.width() / font.scale();
        float h = ( ty2 - ty1 ) * font.height ( ) / font.scale ( );

        if ( !is_space )
        {
          daisy_vtx_t v[] = {
              { { corrected_position.x - 0.5f, corrected_position.y - 0.5f + h, 0.f, 1.f }, color.bgra, { tx1, ty2 } },
              { { corrected_position.x - 0.5f, corrected_position.y - 0.5f, 0.f, 1.f }, color.bgra, { tx1, ty1 } },
              { { corrected_position.x - 0.5f + w, corrected_position.y - 0.5f + h, 0.f, 1.f }, color.bgra, { tx2, ty2 } },
              { { corrected_position.x - 0.5f + w, corrected_position.y - 0.5f, 0.f, 1.f }, color.bgra, { tx2, ty1 } } };

          uint16_t idxs[] = {
              static_cast< uint16_t > ( additional_indices + cont_vertices ),
              static_cast< uint16_t > ( additional_indices + cont_vertices + 1 ),
              static_cast< uint16_t > ( additional_indices + cont_vertices + 2 ),
              static_cast< uint16_t > ( additional_indices + cont_vertices + 3 ),
              static_cast< uint16_t > ( additional_indices + cont_vertices + 2 ),
              static_cast< uint16_t > ( additional_indices + cont_vertices + 1 ),
          };

          memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_vtxs.m_data ) + ( sizeof ( daisy_vtx_t ) * this->m_vtxs.m_size ) ), &v, sizeof ( daisy_vtx_t ) * 4 );
          memcpy ( reinterpret_cast< void * > ( reinterpret_cast< uintptr_t > ( this->m_idxs.m_data ) + ( sizeof ( uint16_t ) * this->m_idxs.m_size ) ), &idxs, sizeof ( uint16_t ) * 6 );

          this->m_vtxs.m_size += 4;
          this->m_idxs.m_size += 6;

          cont_vertices += 4;
          cont_indices += 6;
          cont_primitives += 2;
        }

        corrected_position.x += w - ( 2.f * font.spacing ( ) );
      }

      this->end_batch ( additional_indices, cont_vertices, cont_indices, cont_primitives, font.texture_handle ( ) );
    }
  };
} // namespace daisy

#endif // _SSE2_DAISY_INCLUDE_GUARD
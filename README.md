
![daisy banner; text and background image rendered using daisy itself](https://i.imgur.com/MpxcHkb.jpg)
# daisy
a simple, tiny, very fast, well-documented, Windows only, header-only library for 2D primitive and text rendering using D3D9 & GDI, written in C++17

# showcase
![daisy in action](https://i.imgur.com/37XbM1K.gif)

daisy also runs on linux under Wine without any issues;

![daisy on linux in action](https://i.imgur.com/rjjVajE.gif)

# why
this project was initially intended for a game modification, but it expanded into something that people might find useful on itself (eg. for debug overlays, writing your standalone HW-accelerated Windows application etc.). 

# usage

drag and drop the header file into your project, include it and you're almost set!
quick and dirty usage walkthrough: 
```cpp
// include daisy into your project
#include <daisy.hh>

// somewhere in your main function
daisy::daisy_initialze ( device ); // where device is a pointer to your D3D9 device

// on d3d9 state prepare (see below) 
// if daisy is the only thing you plan on rendering with, you can only call this function once
// if you're implementing daisy alongside in a codebase that changes device state, you should call this every frame before flushing any render queue
daisy::daisy_prepare ( );

// create a font object
daisy::c_fontwrapper font;
if ( !font.create ( "Arial", 24, CLEARTYPE_NATURAL_QUALITY, daisy::FONT_DEFAULT ) )
  // error handling goes here

// create a texture atlas object
daisy::c_texatlas atlas;
if ( !atlas.create ( { width, height } ) ) // where width and height are the dimensions of the atlas texture
   // error handling goes here

// create a normal render queue
daisy::c_renderqueue q;

// or a double buffer queue (if you want to build your queue in a non-rendering thread)
daisy::c_doublebuffer_queue q;

// and actually initialize the queue
if ( !q.create ( max_vertices_capacity, max_indices_capacity ) )
   // error handling goes here

// pushing draw calls into the render queue is very simple
// to note, if using a double buffer queue, q.function_call becomes 
// q.queue( )->function_call.
// see example/example.cc for a deeper dive

// this draws a 100px by 100px white filled rectangle at coords 25px, 25px
q.push_filled_rectangle ( { 25, 25 }, { 100, 100 }, { 255, 255, 255 } );

// this draws the text "daisy is awesome!" at coords 0px, 0px
q.push_text< std::string_view > ( font, { 0, 0 }, "daisy is awesome!", { 255, 255, 255 }, daisy::TEXT_ALIGN_DEFAULT );

// if your render queue is double buffered, you need to swap once you're done filling up the queue with data
q.swap ( );

// in-between your BeginScene and EndScene calls, "flush" your queue to draw to the framebuffer
q.flush ( );

// and clear it if you want to rebuild it with new data
q.clear ( );

// of note; daisy doesn't handle device resets automatically, you need to reset each object yourself
// - all exposed classes inherit from a pure class named c_daisy_resettable_object, which has 1 virtual method, which is 
// virtual bool reset ( bool pre_reset )
// an example reset handler might look something like this
std::vector<std::shared_ptr<c_daisy_resettable_object>> g_daisy_objects; // this holds all daisy objects you create

void my_reset_handler(IDirect3DDevice9* device) {
  for(const auto& iter : g_daisy_objects)
    iter->reset(true); // the argument tells the object that this is taking place pre device-reset, so all D3D9 resources are to be released

  // [...]

  // update the device pointer if needed
  daisy::daisy_initialze ( device ); 
  
  // handle post-reset
  // all D3D9 resources are recreated with the same parameters used on object creation
  // of note; texture atlases will require you to append textures to them again
  for(const auto& iter : g_daisy_objects)
    iter->reset(false); 
}

```
a more in-depth example can be found in example/example.cc. it's heavily recommended that you read the example above and aforementioned file at least once to familiarize yourself with the library. the library itself is also robustly documented and rather straight-forward, so if you get confused feel free to read the code itself.

# caveats/todos
some things you might want to take into consideration before using daisy:

 - the API *could* change at any moment. it probably won't, but it *might* if i'm unhappy with it.
 - daisy relies on some STL containers. (specifically, unordered_map, string_view and it's wide counterpart, vector & array)
 - while i've tried my best to make the API as beginner friendly as possible, expanding daisy might be a bit of a hassle.
 - (more) proper error logging and handling is still on the todo list. for now, at best you get a return value, at worst the library fails silently and continues like nothing happened. (eg. when a glyph isn't supported by a font, the character gets ignored)
 
 there's also a couple of drawbacks to using daisy, but i'm planning on fixing these eventually. nevertheless, the following are currently an issue (pull requests are welcome!):

- vertex and index buffers and texture atlases don't dynamically grow/shrink in size. (this will be fixed soon) 
- each font currently has it's own texture atlas instead of using a shared one (this will be fixed soon) 
- the render queue API should (and will) be expanded with more primitives to draw (currently we have filled rectangles, filled triangles, lines, text.. and that's about it)
- font initialization takes a while.
- shader support is currently rather lackluster.
- the documentation can always be better.

# building daisy
daisy should build with any modern Win32 C++ compiler that supports C++17 (both x86 and x64 builds should work just fine), however support out of the box is only guaranteed for clang 15+ and 2022+ versions of MSVC. the library also shouldn't generate any warnings with `/W4` on clang and `/W3` on MSVC. 

# extra
if you think i missed anything or have any questions, feel free to open an issue. 
if you'd like to contribute, pull requests are always welcome, just try to maintain the code style consistent (i loosely followed Google's [cpp style guide](https://google.github.io/styleguide/cppguide.html), but nothing is set in stone).

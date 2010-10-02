Insight Profiler version 1.0, by Yuriy O'Donnell
================================================

Insight is an instrumented thread profiler with real-time timing data display.
It is mainly geared toward PC games development and only supports Windows C++ applications.
The main goal is to provide real-time game thread timing overview with minimal overhead.
Insight uses rdtsc instruction to get high-precision time information. It has very small cost, 
but is not reliable by any means. Therefore timing output should be considered a rough estimate.
In practice, though, this estimate is pretty close to reality and is good enough for gamedev.

How to use Insight Profiler
===========================

Setup
-----

Insight is an instrumented profiler, which means that you need to explicitly place timing events 
in your game code and link to Insight DLL.

To use Insight, put insight.h and insight.lib where your compiler and linker can find them.
Add insight.lib to the linker input and put insight.dll next to your game executable.

Initialization
--------------

Insight Profiler instance must be initialized before any profile events can be captured.
To do this, Insight::initialize() and Insight::terminate() should be called at your game startup 
and shutdown respectively.

Calling initialize() will create profiler GUI window, which uses DirectX9 for displaying 
timing data. Because of this, it is recommended that you initialize Insight after creating your 
main DirectX device in order for tools like FRAPS and NVidia PerfHUD to work correctly.
Calling terminate() will clean-up all resources used by Insight.

It is possible to initialize Insight in two modes: Synchronous and Asynchronous (recommended).

In Asynchronous mode, Insight will create a thread for itself to handle GUI and event buffers.
Because Insight timing is only supposed to be a rough approximation, overhead from the handler 
thread can be considered negligible.

In Synchronous mode, Insight::update() must be called regularly (every frame) by your game. 
This may produce slightly more accurate timings at the cost of GUI responsiveness.

Using profile events
--------------------

Use Insight::enter() and Insight::exit() to start or finish a timing event.
Calling enter() expects a profile event name as a char* string. This name string must be
static because Insight stores the pointer instead of copying the contents for efficiency.

To simplify typical use cases, Insight::Scope class is provided. It will automatically
call enter() when it is instantiated, hold the profile token and then pass it into exit() when
the instance falls out of scope.

Here is a typical Insight::Scope use example:

void important_work()
{
	Insight::Scope scope_timing("Important work");
	for(int i=0; i<x; ++i)
	{
		do_some_stuff();
	}
}

It is also possible to use __FUNCTION__ preprocessor macro as a profile event identifier, 
which will automatically expand to function name at compile-time.

A recommended way to integrate Insight into your project is to write several helper macros
which would wrap Insight::initialize(), Insight::terminate() and Insight::Scope. For example:

// Profiler.h
#ifdef USE_INSIGHT_PROFILER
	#include <insight.h>
	#pragma comment(lib, "insight.lib")
	#define INSIGHT_INITIALIZE	Insight::initialize();
	#define INSIGHT_TERMINATE	Insight::terminate();
	#define INSIGHT_SCOPE(name) Insight::Scope __insight_scope(name);
#else //USE_INSIGHT_PROFILER
	#define INSIGHT_INITIALIZE
	#define INSIGHT_TERMINATE
	#define INSIGHT_SCOPE(name)
#endif //USE_INSIGHT_PROFILER


Using GUI window
----------------

Insight Profiler has a simple GUI for displaying timing data in real time.
Horizontal bars represent your game threads and vertical bars represent profile events.
When GUI window is not active, it clears and updates every second. But when it has
focus, the update is paused to allow detailed data inspection.

Use left mouse button to drag profile events, mouse wheel to zoom and right button to 
select a timing sub-range.

Closing Insight GUI window will also terminate your game process.

License
=======

Copyright (c) 2010 Yuriy O'Donnell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

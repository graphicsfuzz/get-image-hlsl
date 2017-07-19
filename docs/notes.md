These are my notes on https://github.com/graphicsfuzz/get-image-hlsl for anyone who works on it after me.


# Usage

This is mostly covered in the README.

In general I’ve tried to follow the interneface of get-image-egl, but it has a few differences. Some of these were deliberate choices, others just convenience.

Notably:

1. It’s stricter about bad command line arguments
2. The get_info behaviour is rolled into the same binary with a --get-info flag.
3. It doesn’t support custom vertex shaders
4. The JSON format expected is different

# Developing

Building is currently only supported using Visual Studio 11 (it would be feasible to convert it to e.g. CMake, but as it’s intrinsically windows only there’s not much point). You can also compile it from the command line using MSBuild and a developer shell, so it would probably be relatively easy to set up automated builds on appveyor, but we currently don’t have any.

Note that the build depends on nuget (https://www.nuget.org/). This is included as part of Visual Studio 11. We use it to get DirectXTK which provides our screengrab routines (https://github.com/Microsoft/DirectXTK/wiki/ScreenGrab) that we use for generating the actual image. This is configured here: https://github.com/graphicsfuzz/get-image-hlsl/blob/master/get-image-hlsl/packages.config

## Design

The core design is more or less the same as that for get-image-egl.

We create a vertex shader (hard coded) that just maps through 2D points and sets the colour to white. We also load a pixel shader (fragment shader in OpenGL parlance)

We then draw two triangles that cover the window, causing the pixel shader to run for every point.

One notable detail is that what we turn into an image is technically not the screen. We render to the back buffer, present it (i.e. render it to the screen. I think this is necessary to flush the render pipeline or something? The internet claims that it’s possible to do without this, but I couldn’t make it work), and then get the back buffer as a texture which we then convert to a png using DirectXTK.

One big difference is the way we interact with pass data to shaders. OpenGL shaders can be introspected to find out what uniforms they require, and uniform data is passed by name. DirectX lacks this capability - instead arguments are passed positionally. They’re also expected to be laid out as structs defined in C++ source code to get the alignment right.

This significantly limits what we can do in comparison to OpenGL

Currently the only uniform we pass is injectionSwitch. If it is present in the JSON we pass an appropriate constant buffer to set the values.

Better support in line with that in get-image-egl would require parsing the HLSL ourselves to figure out the right positions/structure are.

We could also just encode the values to pass positionally as appropriately laid out data in the JSON. This will be a little tricky but not impossible - the most important thing to get right is the packing rules. See: https://msdn.microsoft.com/en-us/library/windows/desktop/bb509632(v=vs.85).aspx

I suspect there are lots of fun details that will emerge that get handled automatically by doing this the expected way.

## Debugging

TLDR: If things are behaving in a confusing way and not emitting any useful error messages, launch the tool under a debugger from visual studio. It’s awkward, but it will give you more error messages in the debugging console (How to specify command line arguments: https://msdn.microsoft.com/en-us/library/cs8hbt1w(v=vs.90).aspx )

Basically DirectX will not emit useful error messages unless running under a debugger. There is literally no sensible way to get it to print its error messages to stderr like any normal human being would want. The general consensus from Microsoft developers seems to be “Huh. Why would you want to print error messages to stderr?”

I asked about this on stackoverflow and was vaguely pointed in the direction of the ID3D11InfoQueue interface (https://msdn.microsoft.com/en-us/library/windows/desktop/ff476538(v=vs.85).aspx) and told that it might be possible to do it that way, but at that point I’d lost the will to live so I didn’t investigate further.

## Additional Notes

### Code quality

Basically, it’s bad. This is some of the worst code I’ve ever written, largely because it’s cobbled together from tutorials. Every DirectX tutorial starts with “It’s easy! Just copy and paste these thousand lines of code”, and after many uphill struggles I eventually gave in and followed their advice.

I spent a long time trying to understand things from first principles and actually get to grips with the DirectX API in order to design this properly. It didn’t work. All DirectX documentation and tutorials fight against you doing this, and DirectX gives such bad error messages that the result is almost impossible to debug when it goes wrong.

I made some feeble attempts at trying to tidy it up, but given how brittle the DirectX API is and how difficult failure to debug was I gave up pretty quickly.

In the unfortunate event that you grow to learn more about Directx, I recommend a significant refactoring and rewriting of the code. Sorry.

### DirectX versions

The astute observer will notice that this is written in DirectX 11, but DirectX 12 is out.

The reason for this is that for most usage even Microsoft don’t recommend DirectX 12. DirectX 12 is effectively “expert mode DirectX”, while DirectX 11 is still in active development and recommended for normal usage.

### Windowing

The code sure looks like it should pop up a window when it runs, doesn’t it?

I’m as confused as you are about why it doesn’t. Or, more precisely, I’m confused about why it doesn’t and yet still manages to produce an image. Fortunately this was exactly the behaviour we wanted and given my earlier experiences I thought it best not to ask why of anything to do with DirectX.

Unlike the traditional `“int main(int, **char)”` entry point for C++, Windows offers you an embarrassment of different possible entry points. We’re using the wmain one, which is unicode aware. There is also the WINMAIN function that you are expected to use for things that need to create windows.

In order to get better command line behaviour I converted the code from WINMAIN  to a wmain. I believe this is supposed to work, but in the course of doing so the window stopped appearing. I verified that an image was still created, so I decided not to worry about it.

This may have longer term repercussions. As far as I can tell it really is using the graphics card and the right rendering types, but DirectX might be lying to me.

It may also mean that a significant amount of this code is secretly dead and could be removed. That didn’t appear to be the case on experimentation, but it might be something more subtle.

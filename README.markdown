# get-image-hlsl

This is a tool that takes a shader written in Microsoft's
[High Level Shading Language](https://en.wikipedia.org/wiki/High-Level_Shading_Language)
and converts it into an image.

Current limitations:

* The generated image is always a png (regardless of extension)
* The generated image is always 256x256
* It has a single hard coded vertex shader that just passes the
  position through to the pixel shader verbatim and a colour of white.
* It does not support passing data to the shader via uniforms.

The first two would be pretty easy to fix, the latter two should be
possible to fix but are somewhat harder.

Usage:

```bash
get-image-hlsl.exe SamplePixelShader.hlsl --output sometarget.png
```

# Building

This just builds in visual studio for the moment, and is written using
Visual Studio 2017. (I don't think this will actually work with an older
version of visual studio, but I haven't tried).

It requires DirectXTK, which NuGet *should* install for you automatically,
but if you're seeing missing header warnings that's probably why. If you're
using an older version of visual studio then you may not have NuGet installed.

Long term we'll want to at least get a basic command line build working, but
it will probably always still be building using the visual studio tool chain:
This is very much a Windows only project, because the thing it's designed
to target only exists on Windows!

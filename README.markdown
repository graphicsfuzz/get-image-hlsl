# get-image-hlsl

Long-term this will be a tool that takes a shader written in Microsoft's
[High Level Shading Language](https://en.wikipedia.org/wiki/High-Level_Shading_Language)
and converts it into an image in a desired target format.

Short-term, it's a prototype which can just about manage to compile and load
a shader. It does however at least let you validate whether a shader written
in HLSL compiles correctly, so there's that?

Watch this space and hopefully it will become more useful.

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

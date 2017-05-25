// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _WIN32_WINNT 0x600

#include "targetver.h"

#include <iostream>
#include <stdio.h>

#include <tchar.h>

#include <d3dcompiler.h>

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <Windows.h>
#include <concrt.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <d3d11_3.h>
#include <dwrite_3.h>
#include <memory>
#include <wincodec.h>
#include <wrl/client.h>

#include <codecvt>
#include <string>

// " to simplify the tutorial we will go ahead and add them all to your new
// project's pch.h header" lol

#include "CommonStates.h"
#include "DDSTextureLoader.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "GamePad.h"
#include "GeometricPrimitive.h"
#include "GraphicsMemory.h"
#include "Keyboard.h"
#include "Model.h"
#include "Mouse.h"
#include "PrimitiveBatch.h"
#include "ScreenGrab.h"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "VertexTypes.h"
#include "WICTextureLoader.h"

#include <comdef.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")

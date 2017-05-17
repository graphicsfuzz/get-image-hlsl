#include "stdafx.h"
#include "get-image-hlsl.h"

using namespace Microsoft::WRL;
using namespace DirectX;


ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader*     g_pVertexShader = nullptr;
ID3D11PixelShader*      g_pPixelShader = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
ID3D11DeviceContext1*   g_pImmediateContext1 = nullptr;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitDevice(std::wstring);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);


void checkFailImpl(HRESULT hr, int lineno) {
	if (FAILED(hr)) {
		std::cerr << "Failed at line " << lineno << std::endl;
		LPWSTR output;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&output, 0, NULL);
		std::wcerr << output << std::endl;
		exit(1);
	}
}

#define checkFail(hr) checkFailImpl(hr, __LINE__)

void PrintErrorBlob(ID3DBlob * errorBlob)
{
	if (errorBlob) {
		// FIXME: This is incredibly stupid code and I can imagine no earthly
		// reason for it to be necessary. All of the obvious things to do here
		// seemed to produce no output when run, but that must be the result of
		// me doing something wrong. However, this works for now.
		auto n = errorBlob->GetBufferSize();
		auto err = (char *)errorBlob->GetBufferPointer();
		for (auto i = 0UL; i < n; i++) {
			if (!err[i])
				break;
			std::cerr << err[i];
		}
		std::cerr << std::endl;
		errorBlob->Release();
	}
}


const char* vertex_shader_source =
"struct VertexShaderInput { float2 position: POSITION; };\n"
"struct PixelShaderInput { float4 position : SV_POSITION; float3 colour : COLOR0;};\n"
"\n"
"PixelShaderInput main(VertexShaderInput input) {\n"
"  PixelShaderInput output;\n"
"  output.position = float4(input.position, 0.0, 1.0);\n"
"  output.colour = float3(256, 256, 256);\n"
"  return output;\n"
"};\n";

struct VertexShaderInput {
	DirectX::XMFLOAT2 position;
};

void CompileShaderFromFile(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
	_In_ LPCSTR profile, _Outptr_ ID3DBlob **blob) {
	if (!srcFile || !entryPoint || !profile || !blob)
		exit(1);

	*blob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;

	const D3D_SHADER_MACRO defines[] = { NULL, NULL };

	ID3DBlob *errorBlob = nullptr;
	HRESULT hr =
		D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint, profile, flags, 0, blob, &errorBlob);
	if (FAILED(hr)) {
		PrintErrorBlob(errorBlob);

		checkFail(hr);
	}
}

void CompileShaderStr(const char *srcCode, _In_ LPCSTR entryPoint,
	_In_ LPCSTR profile, _Outptr_ ID3DBlob **blob) {
	if (!srcCode || !entryPoint || !profile || !blob)
		exit(1);

	*blob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;

	const D3D_SHADER_MACRO defines[] = { NULL, NULL };

	ID3DBlob *errorBlob = nullptr;
	HRESULT hr =
		D3DCompile(srcCode, strlen(srcCode), "<string>", defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint, profile, flags, 0, blob, &errorBlob);
	if (FAILED(hr)) {
		PrintErrorBlob(errorBlob);

		checkFail(hr);
	}
}

int wmain(int argc, wchar_t* argv[], wchar_t *envp[]) {
	std::wstring pixel_shader;
	std::wstring output(L"output.png");

	for (int i = 1; i < argc; i++) {
		std::wstring curr_arg = std::wstring(argv[i]);
		if (!curr_arg.compare(0, 2, L"--")) {
			if (curr_arg == L"--output") {
				output = argv[++i];
				continue;
			}

			std::wcerr << "Unknown argument " << curr_arg << std::endl;
			continue;

		}
		if (pixel_shader.length() == 0) {
			pixel_shader = curr_arg;
		}
		else {
			std::wcerr << "Ignoring extra argument " << curr_arg << std::endl;
		}
	}

	if (pixel_shader.length() == 0) {
		std::wcerr << "Requires pixel shader argument!" << std::endl;
		return EXIT_FAILURE;
	}

	checkFail(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));
	checkFail(InitDevice(pixel_shader));

	// Clear the back buffer 
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);

	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
	g_pImmediateContext->Draw(3, 0);
	g_pImmediateContext->Draw(3, 3);

	// Present the information rendered to the back buffer to the front buffer (the screen)
	g_pSwapChain->Present(0, 0);

	ComPtr<ID3D11Texture2D> backBuffer;
	checkFail(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		reinterpret_cast<LPVOID*>(backBuffer.GetAddressOf())));
	checkFail(SaveWICTextureToFile(g_pImmediateContext, backBuffer.Get(),
		GUID_ContainerFormatPng, output.c_str()));

	return 0;
}


struct SimpleVertex
{
	XMFLOAT3 Pos;
};

HRESULT InitDevice(std::wstring pixel_shader)
{
	HWND                    g_hWnd = nullptr;
	D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
	ID3D11Device*           g_pd3dDevice = nullptr;
	ID3D11Device1*          g_pd3dDevice1 = nullptr;
	IDXGISwapChain1*        g_pSwapChain1 = nullptr;

	ID3D11InputLayout*      g_pVertexLayout = nullptr;
	ID3D11Buffer*           g_pVertexBuffer = nullptr;

	HINSTANCE hInstance = GetModuleHandle(NULL);
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"GetImageHLSL";
	wcex.hIconSm = NULL;
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	RECT rc = { 0, 0, 800, 600 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	// For unknown reasons this window does not actually get shown. It probably
	// should - it does if we try to compile this as a windows app rather than a 
	// command line one - but it doesn't. Fortunately that's exactly what we want!
	// But this is still surprising.
	g_hWnd = CreateWindow(L"GetImageHLSL", L"You probably should never see this",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return E_FAIL;

	HRESULT hr = S_OK;

	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
		if (SUCCEEDED(hr))
		{
			hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
		}

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = g_hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	}

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// Compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;
	CompileShaderStr(vertex_shader_source, "main", "vs_4_0", &pVSBlob);

	// Create the vertex shader
	checkFail(
		g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader));

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	CompileShaderFromFile(pixel_shader.c_str(), "main", "ps_4_0", &pPSBlob);

	// Create the pixel shader
	checkFail(
		g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader));

	// Create vertex buffer
	SimpleVertex vertices[] =
	{
		XMFLOAT3(-1.0f, -1.0f, 0.5f),
		XMFLOAT3(-1.0f, 1.0f, 0.5f),
		XMFLOAT3(1.0f, -1.0f, 0.5f),

		XMFLOAT3(1.0f, 1.0f, 0.5f),
		XMFLOAT3(1.0f, -1.0f, 0.5f),
		XMFLOAT3(-1.0f, 1.0f, 0.5f),

	};
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(vertices);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;
	checkFail(g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer));

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

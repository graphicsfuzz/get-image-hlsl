#include "stdafx.h"

#define JSON_NOEXCEPTION
#include "json.hpp"

using json = nlohmann::json;
using namespace Microsoft::WRL;
using namespace DirectX;

const UINT WIDTH = 256;
const UINT HEIGHT = 256;

ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader*     g_pVertexShader = nullptr;
ID3D11PixelShader*      g_pPixelShader = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
ID3D11DeviceContext1*   g_pImmediateContext1 = nullptr;
ID3D11Device*           g_pd3dDevice = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device1*			g_pd3dDevice1 = nullptr;
IDXGISwapChain1*        g_pSwapChain1 = nullptr;

ID3D11InputLayout*      g_pVertexLayout = nullptr;
ID3D11Buffer*           g_pVertexBuffer = nullptr;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitDevice(UINT, D3D_DRIVER_TYPE*);
void LoadShaders(std::wstring &pixel_shader, json&);
void OutputPixelShader(std::wstring&, json&, std::wstring&);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void checkFailImpl(HRESULT, int);
void CompileShaderStr(const char *srcCode, _In_ LPCSTR entryPoint,
	_In_ LPCSTR profile, _Outptr_ ID3DBlob **blob);
void CompileShaderFromFile(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
	_In_ LPCSTR profile, _Outptr_ ID3DBlob **blob);
void PrintDeviceInfo();
bool readFile(const std::wstring& fileName, std::string& contentsOut);
#define checkFail(hr) checkFailImpl(hr, __LINE__)

int wmain(int argc, wchar_t* argv[], wchar_t *envp[]) {
	std::wstring pixel_shader;
	std::wstring output(L"output.png");
	D3D_DRIVER_TYPE force_driver_type = D3D_DRIVER_TYPE_UNKNOWN;
	bool print_adapter_info = false;

	for (int i = 1; i < argc; i++) {
		std::wstring curr_arg = std::wstring(argv[i]);
		if (!curr_arg.compare(0, 2, L"--")) {
			if (curr_arg == L"--output") {
				output = argv[++i];
				continue;
			}
			if (curr_arg == L"--get-info"){
				print_adapter_info = true;
				continue;
			}
			if (curr_arg == L"--driver") {
				std::wstring driver_string = argv[++i];
				if (driver_string == L"auto") {
					continue;
				}
				else if (driver_string == L"hardware") {
					force_driver_type = D3D_DRIVER_TYPE_HARDWARE;
				}
				else if (driver_string == L"warp") {
					force_driver_type = D3D_DRIVER_TYPE_WARP;
				}
				else if (driver_string == L"reference") {
					force_driver_type = D3D_DRIVER_TYPE_REFERENCE;
				}
				else {
					std::wcerr << "Unknown driver specification  " << driver_string <<
						" expected one of auto, hardware, warp, reference" << std::endl;
					return EXIT_FAILURE;
				}
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

	if (!print_adapter_info && (pixel_shader.length() == 0)) {
		std::wcerr << "Requires pixel shader argument or --get-info" << std::endl;
		return EXIT_FAILURE;
	}
	if (print_adapter_info && (pixel_shader.length() > 0)) {
		std::wcerr << "Cannot specify both --get-info and pixel shader argument" << std::endl;
		return EXIT_FAILURE;
	}

	checkFail(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));

	if (force_driver_type == D3D_DRIVER_TYPE_UNKNOWN) {
		D3D_DRIVER_TYPE driverTypes[] =
		{
			D3D_DRIVER_TYPE_HARDWARE,
			D3D_DRIVER_TYPE_WARP,
			D3D_DRIVER_TYPE_REFERENCE,
		};
		UINT numDriverTypes = ARRAYSIZE(driverTypes);

		checkFail(InitDevice(numDriverTypes, driverTypes));
	}
	else {
		checkFail(InitDevice(1, &force_driver_type));
	}

	json uniform_data;

	if (print_adapter_info) {
		PrintDeviceInfo();
		return EXIT_SUCCESS;
	}


	assert(pixel_shader.size() > 0);
	std::wstring jsonFilename(pixel_shader);
	jsonFilename.replace(jsonFilename.end() - 4, jsonFilename.end(), L"json");
	std::string jsonContent;
	if (readFile(jsonFilename, jsonContent)) {
		uniform_data = json::parse(jsonContent);
	}

	OutputPixelShader(pixel_shader, uniform_data, output);

	return EXIT_SUCCESS;
}

void OutputPixelShader(std::wstring &pixel_shader, json& uniform_data, std::wstring &output)
{
	/*	
	Take our pixel shader and write it to an output image as a PNG.
	*/

	LoadShaders(pixel_shader, uniform_data);

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
}

struct SimpleVertex
{
	XMFLOAT3 Pos;
};


const char* vertex_shader_source =
"struct VertexShaderInput { float2 position: POSITION; };\n"
"struct PixelShaderInput { float4 position : SV_POSITION; float3 colour : COLOR0;};\n"
"\n"
"PixelShaderInput main(VertexShaderInput input) {\n"
"  PixelShaderInput output;\n"
"  output.position = float4(input.position, 0.0, 1.0);\n"
"  output.colour = float3(1, 1, 1);\n"
"  return output;\n"
"};\n";



HRESULT InitDevice(UINT numDriverTypes, D3D_DRIVER_TYPE *driverTypes)
{
	/*
	This function does all the set up we need to actually produce our image.

	It's very much cobbled together from tutorials and I'd be lying if I said I fully
	understood it.
	
	*/
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

	RECT rc = { 0, 0, WIDTH, HEIGHT};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	// For unknown reasons this window does not actually get shown. It probably
	// should - it does if we try to compile this as a windows app rather than a 
	// command line one - but it doesn't. Fortunately that's exactly what we want!
	// But this is still surprising.
	g_hWnd = CreateWindow(L"GetImageHLSL", L"You probably should never see this",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return E_FAIL;

	HRESULT hr = S_OK;

	UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;


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

		if (SUCCEEDED(hr)) {
			break;
		}
	}
	checkFail(hr);

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			checkFail(dxgiDevice->GetAdapter(&adapter));
			checkFail(adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory)));
			adapter->Release();
			dxgiDevice->Release();
		}
	}


	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(g_pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = WIDTH;
		sd.Height = HEIGHT;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		checkFail(dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1));
		checkFail(g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain)));

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = WIDTH;
		sd.BufferDesc.Height = HEIGHT;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = g_hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		checkFail(dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain));
	}

	dxgiFactory->Release();

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	checkFail(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer)));

	checkFail(g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView));
	pBackBuffer->Release();

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)WIDTH;
	vp.Height = (FLOAT)HEIGHT;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	return S_OK;
}


struct InjectionSwitch {
	DirectX::XMFLOAT2 injectionSwitch;
};

void LoadShaders(std::wstring &pixel_shader, json& uniform_data)
{

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
	checkFail(g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout));
	pVSBlob->Release();

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Compile the pixel shader
	if (pixel_shader.size() > 0) {
		ID3DBlob* pPSBlob = nullptr;
		CompileShaderFromFile(pixel_shader.c_str(), "main", "ps_4_0", &pPSBlob);

		// Create the pixel shader
		checkFail(
			g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader));
	}

	// Create vertex buffer with two separate triangles, each covering half
	// of the screen. We use this to just cover all of the screen so that the pixel shader
	// gets run.
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

	if (uniform_data.is_object() && false) {
		if (uniform_data.count("injectionSwitch") > 0) {
			json injection_switch_json = uniform_data.at("injectionSwitch");
			if (
				!injection_switch_json.is_array() ||
				injection_switch_json.size() != 2 ||
				!injection_switch_json.at(0).is_number() ||
				!injection_switch_json.at(1).is_number()
			) {
				std::cerr << "injectionSwitch should be an array of two floats but got " << injection_switch_json << " instead." << std::endl;
				exit(EXIT_FAILURE);
			}

			InjectionSwitch injection_switch = {
				DirectX::XMFLOAT2(injection_switch_json.at(0), injection_switch_json.at(1))
			};

			ID3D11Buffer *buffer;

			D3D11_BUFFER_DESC buffer_description;
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(InjectionSwitch);
			bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA init_data;
			InitData.pSysMem = &injection_switch;
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;
			
			checkFail(g_pd3dDevice->CreateBuffer(&bd, &init_data, &buffer));
		}
	}
}


// convert wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

void PrintDeviceInfo() {

	IDXGIDevice *dxgiDevice;
	assert(g_pd3dDevice != nullptr);
	g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
	{
		ComPtr<IDXGIAdapter> adapter;
		if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
		{
			DXGI_ADAPTER_DESC desc;
			if (SUCCEEDED(adapter->GetDesc(&desc)))
			{
				json j = {
					{ "Description", wstring_to_utf8(std::wstring(desc.Description)) },
					{ "VendorId", desc.VendorId},
					{ "VendorId", desc.VendorId },
					{ "DeviceId", desc.DeviceId },
					{ "SubSysId", desc.SubSysId },
					{ "Revision", desc.Revision },
					{ "DedicatedVideoMemory", desc.DedicatedVideoMemory },
					{ "DedicatedSystemMemory", desc.DedicatedSystemMemory },
					{ "SharedSystemMemory", desc.SharedSystemMemory },
				};
				std::cout << j.dump(4) << std::endl;
			}
		}
	}
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

struct VertexShaderInput {
	DirectX::XMFLOAT2 position;
};


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


bool readFile(const std::wstring& fileName, std::string& contentsOut){
	std::ifstream ifs(fileName.c_str());
	if (!ifs) {
		return false;
	}
	std::stringstream ss;
	ss << ifs.rdbuf();
	contentsOut = ss.str();
	return true;
}


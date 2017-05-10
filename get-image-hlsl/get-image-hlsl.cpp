#include "stdafx.h"
#include "get-image-hlsl.h"

using namespace Microsoft::WRL;
using namespace DirectX;


void checkFail(HRESULT hr) {
	if (FAILED(hr)) {
		LPWSTR output;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&output, 0, NULL);
		std::wcerr << output << std::endl;
		exit(1);
	}
}


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
"struct PixelShaderInput { float4 position : SV_POSITION; };\n"
"\n"
"PixelShaderInput main(VertexShaderInput input) {\n"
"  PixelShaderInput output;\n"
"  output.position = float4(input.position, 0.0, 1.0);\n"
"  return output;\n"
"};\n";

struct VertexShaderInput {
	DirectX::XMFLOAT2 position;
};

void CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
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

#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
	checkFail(Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(
		RO_INIT_MULTITHREADED));
#else
	checkFail(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));
#endif

	ComPtr<ID3D11Device> _device;
	ComPtr<ID3D11DeviceContext> _context;

	D3D_FEATURE_LEVEL featureLevel;

	checkFail(
		D3D11CreateDevice(
			nullptr, /* Adapter: The adapter (video card) we want to use. We may
						use NULL to pick the default adapter. */
			D3D_DRIVER_TYPE_HARDWARE, /* DriverType: We use the GPU as backing
										 device. */
			NULL, /* Software: we're using a D3D_DRIVER_TYPE_HARDWARE so it's not
					 applicaple. */
			D3D11_CREATE_DEVICE_DEBUG,
			NULL, /* Feature Levels (ptr to array):  what version to use. */
			0,    /* Number of feature levels. */
			D3D11_SDK_VERSION,      /* The SDK version, use D3D11_SDK_VERSION */
			_device.GetAddressOf(), /* OUT: the ID3D11Device object. */
			&featureLevel,          /* OUT: the selected feature level. */
			_context.GetAddressOf())); /* OUT: the ID3D11DeviceContext that
										  represents the above features. */

	ComPtr<ID3D11Device3> device;
	checkFail(_device.As(&device));

	ComPtr<ID3D11DeviceContext3> context;
	checkFail(_context.As(&context));

	assert(device.Get());
	assert(context.Get());

	ID3DBlob *psBlob = nullptr;
	CompileShader(pixel_shader.c_str(), "main", "ps_4_0_level_9_1", &psBlob);

	// First compile and attach the pixel shader.
	ID3D11PixelShader *pixelShader = nullptr;
	checkFail(_device->CreatePixelShader(psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(), nullptr,
		&pixelShader));

	ID3D11VertexShader *vertexShader = nullptr;
	ID3DBlob *vsBlob = nullptr;
	CompileShaderStr(vertex_shader_source, "main", "vs_4_0_level_9_1", &vsBlob);
	checkFail(_device->CreateVertexShader(vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(), nullptr,
		&vertexShader));

	VertexShaderInput vertices[] = {
		{{-1.0f,  1.0f}},
		{{-1.0f, -1.0f}},
		{{ 1.0f, -1.0f}},
		{{1.0f,  1.0f}}
	};

	ID3D11Buffer *vertexBuffer;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VertexShaderInput) * 3;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	device->CreateBuffer(&bd, NULL, &vertexBuffer);

	D3D11_MAPPED_SUBRESOURCE ms;
	context->Map(vertexBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
	memcpy(ms.pData, vertices, sizeof(vertices));
	context->Unmap(vertexBuffer, NULL);

	D3D11_INPUT_ELEMENT_DESC inputDescription[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D11InputLayout *inputLayout;
	device->CreateInputLayout(inputDescription, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
	context->IASetInputLayout(inputLayout);

	// Create the render target texture
	D3D11_TEXTURE2D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));
	textureDesc.Width = 256;
	textureDesc.Height = 256;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D *renderTarget = NULL;
	device->CreateTexture2D(&textureDesc, NULL, &renderTarget);
	assert(renderTarget != nullptr);

	D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
	rtDesc.Format = textureDesc.Format;
	rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtDesc.Texture2D.MipSlice = 0;

	ID3D11RenderTargetView *pRenderTargetView = NULL;
	checkFail(device->CreateRenderTargetView(renderTarget, &rtDesc, &pRenderTargetView));

	assert(pRenderTargetView != nullptr);

	context->OMSetRenderTargets(1, &pRenderTargetView, nullptr);

	context->PSSetShader(pixelShader, nullptr, 0);

	context->VSSetShader(vertexShader, nullptr, 0);


	ID3D11Buffer *indexBuffer = NULL;

	// Create indices.
	unsigned int indices[] = {
		0, 1, 2,
		2, 3, 0
	};

	// Fill in a buffer description.
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(unsigned int) * 3;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	// Define the resource data.
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = indices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	// Create the buffer with the device.
	checkFail(device->CreateBuffer(&bufferDesc, &InitData, &indexBuffer));

	// Set the buffer.
	context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	context->DrawIndexed(
		2,
		0,
		0
	);
	context->Flush();

	checkFail(SaveWICTextureToFile(context.Get(), renderTarget,
		GUID_ContainerFormatJpeg, output.c_str()));

	std::cerr << "OK" << std::endl;

	return 0;
}

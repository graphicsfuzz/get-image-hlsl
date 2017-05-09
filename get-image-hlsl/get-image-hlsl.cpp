#include "stdafx.h"

using namespace Microsoft::WRL;

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

void CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
                   _In_ LPCSTR profile, _Outptr_ ID3DBlob **blob) {
  if (!srcFile || !entryPoint || !profile || !blob)
    exit(1);

  *blob = nullptr;

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;

  const D3D_SHADER_MACRO defines[] = {NULL, NULL};

  ID3DBlob *errorBlob = nullptr;
  HRESULT hr =
      D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                         entryPoint, profile, flags, 0, blob, &errorBlob);
  if (FAILED(hr)) {
    if (errorBlob) {
      // FIXME: This is incredibly stupid code and I can imagine no earthly
      // reason
      // for it to be necessary. All of the obvious things to do here seemed to
      // produce no output when run, but that must be the result of me doing
      // something wrong. However, this works for now.
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

    checkFail(hr);
  }
}

int main() {

#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
  checkFail(Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(
      RO_INIT_MULTITHREADED));
#else
  checkFail(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));
#endif

  ID3DBlob *psBlob = nullptr;
  CompileShader(L"simple.hlsl", "main", "vs_4_0_level_9_1", &psBlob);
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

  ID3D11VertexShader *vertexShader;

  checkFail(_device->CreateVertexShader(psBlob->GetBufferPointer(),
                                        psBlob->GetBufferSize(), nullptr,
                                        &vertexShader));

  // Attach our vertex shader.
  context->VSSetShader(vertexShader, nullptr, 0);

  // Create the render target texture
  D3D11_TEXTURE2D_DESC desc;
  ZeroMemory(&desc, sizeof(desc));
  desc.Width = 256;
  desc.Height = 256;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

  ID3D11Texture2D *pRenderTarget = NULL;
  device->CreateTexture2D(&desc, NULL, &pRenderTarget);

  D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
  rtDesc.Format = desc.Format;
  rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Texture2D.MipSlice = 0;

  ID3D11RenderTargetView *pRenderTargetView = NULL;
  device->CreateRenderTargetView(pRenderTarget, &rtDesc, &pRenderTargetView);

  psBlob->Release();

  return 0;
}

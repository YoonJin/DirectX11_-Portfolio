#include "pch.h"
#include "Texture.h"

bool Texture::LoadFromFile(Graphics* graphics, IWICImagingFactory* wicFactory, const std::wstring& path)
{
	// [WIC (Windows Imaging Component)]
	// PNG/JPG/BMP 디코딩을 OS 가 제공한다. 별도 라이브러리 없이 PNG 를 읽는 가장 단순한 방법이다.
	// WIC 팩토리는 COM 객체라 CoInitializeEx 가 되어 있어야 한다 (WinMain 에서 한 번). D3D 디바이스는 COM 스타일이지만
	// CoInitialize 가 필요 없다는 점이 헷갈리기 쉽다 — MyWindows.cpp 주석 참고.
	// (5단계 SpriteBatch::LoadTexture 를 그대로 옮긴 코드다 — 동작 변화 0 이 이 단계의 규율이다)
	//
	// 흐름 : 팩토리 → 파일 디코더 → 프레임 0 → 포맷 변환기(32bppPRGBA) → CopyPixels → CreateFromMemory
	if (!wicFactory) return false;

	ComPtr<IWICBitmapDecoder> decoder;
	HRESULT hr = wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
	if (FAILED(hr)) return false;   // 파일 없음/손상 — 호출자(TextureManager)가 절대 경로를 찍는다

	ComPtr<IWICBitmapFrameDecode> frame;
	hr = decoder->GetFrame(0, frame.GetAddressOf());
	CHECK(hr);
	if (FAILED(hr)) return false;

	// [프리멀티플라이드를 로드 시점에 결정한다]
	// GUID_WICPixelFormat32bppPRGBA : R,G,B,A 바이트 순서 + RGB 에 A 가 미리 곱해진 형식.
	// DXGI_FORMAT_R8G8B8A8_UNORM 과 바이트 순서가 같아서 CopyPixels 결과를 그대로 텍스처 초기 데이터로 넣을 수 있다.
	// (PNG 원본은 보통 straight 알파다. 변환기가 곱셈을 해준다. 셰이더에서 곱하는 방식도 있지만 로드 시 한 번 하면
	//  셰이더와 블렌드 상태가 단순해진다)
	ComPtr<IWICFormatConverter> converter;
	hr = wicFactory->CreateFormatConverter(converter.GetAddressOf());
	CHECK(hr);
	if (FAILED(hr)) return false;
	hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
	CHECK(hr);
	if (FAILED(hr)) return false;

	UINT width = 0, height = 0;
	hr = converter->GetSize(&width, &height);
	CHECK(hr);
	if (FAILED(hr) || width == 0 || height == 0) return false;

	const UINT stride = width * 4;   // 32bpp
	std::vector<uint8_t> pixels(static_cast<size_t>(stride) * height);
	hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
	CHECK(hr);
	if (FAILED(hr)) return false;

	return CreateFromMemory(graphics, width, height, pixels.data());
}

bool Texture::CreateFromMemory(Graphics* graphics, uint width, uint height, const void* rgba8Pixels)
{
	if (!graphics || !rgba8Pixels || width == 0 || height == 0) return false;
	ID3D11Device* device = graphics->GetDevice();

	// IMMUTABLE : 생성 시 초기 데이터로 채우고 다시는 쓰지 않는다. 드라이버가 가장 빠른 메모리에 둘 수 있다.
	// 255x255 같은 NPOT 크기도 feature level 10_0 이상에서는 제한 없이 지원된다 (밉맵, WRAP 주소 모드 포함).
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;   // 밉맵 없음 (2D 스프라이트는 원본 해상도 근처에서만 그린다)
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA init = {};
	init.pSysMem = rgba8Pixels;
	init.SysMemPitch = width * 4;

	// 재생성 가능성(같은 객체에 두 번 Load)은 관리자가 막지만, 만약을 위해 기존 값을 먼저 놓는다 (ReleaseAndGetAddressOf).
	HRESULT hr = device->CreateTexture2D(&desc, &init, _texture.ReleaseAndGetAddressOf());
	CHECK(hr);
	if (FAILED(hr)) return false;

	// SRV : 셰이더가 텍스처를 "읽는 방법". RTV 가 "쓰는 방법" 이었던 것과 대칭이다. nullptr desc = 텍스처 포맷/전체 밉 그대로.
	hr = device->CreateShaderResourceView(_texture.Get(), nullptr, _srv.ReleaseAndGetAddressOf());
	CHECK(hr);
	if (FAILED(hr))
	{
		_texture.Reset();
		return false;
	}

	_width = width;
	_height = height;
	return true;
}

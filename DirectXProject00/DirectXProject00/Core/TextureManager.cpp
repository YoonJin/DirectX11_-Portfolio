#include "pch.h"
#include "TextureManager.h"
#include "Path.h"

TextureManager::TextureManager(Graphics* graphics)
	: _graphics(graphics)
{
	// WIC 팩토리는 한 번만 만든다. 5단계에서는 LoadTexture 마다 CoCreateInstance 했다 — 동작은 같고 비용만 줄었다.
	// CoInitializeEx 가 안 되어 있으면 여기서 CO_E_NOTINITIALIZED 로 실패한다 (WinMain 이 먼저 부른다).
	HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(_wicFactory.GetAddressOf()));
	CHECK(hr);
	// 실패해도 생성자는 끝낸다 — Load() 가 팩토리 null 을 보고 false 를 돌려주고, 호출자가 nullptr 를 처리한다.
	// 원인을 여기서 한 번 찍어 둔다. 아니면 이후의 모든 Load 실패 메시지가 "파일 문제" 로 읽힌다.
	if (FAILED(hr)) ::OutputDebugStringW(L"[TextureManager] WIC factory creation failed (CoInitializeEx missing?) — every Load() will fail\n");
}

TextureManager::~TextureManager()
{
	// 멤버 선언 역순 : _white → _textures(모든 Texture) → _wicFactory. 전부 Graphics 보다 먼저다.
	// unique_ptr 가 Texture 를 지우면 그 안의 ComPtr 이 SRV/Texture2D 를 Release 한다 — 여기서 할 일은 없다.
}

Texture* TextureManager::Load(const std::wstring& relativePath)
{
	// 정규화된 절대 경로로 파일을 열고, 그 소문자판을 키로 쓴다 (Path.h 의 Normalize/MakeKey).
	const std::wstring path = Path::Normalize(relativePath);
	const std::wstring key = Path::MakeKey(path);

	// 캐시 적중 — 이 관리자가 존재하는 이유. 같은 파일을 두 번 요청하면 같은 포인터다.
	auto found = _textures.find(key);
	if (found != _textures.end()) return found->second.get();

	auto texture = std::make_unique<Texture>();
	if (!texture->LoadFromFile(_graphics, _wicFactory.Get(), path))
	{
		// 경로 문제가 가장 흔한 실패라 메시지에 절대 경로가 있어야 한다. CHECK(HRESULT) 가 아니라 여기서 잡는다.
		// assert 는 걸지 않는다 — "존재하지 않는 경로 → nullptr, 크래시 없음" 이 이 함수의 계약이다. 호출자가 판단한다.
		::OutputDebugStringW((L"[TextureManager] failed to load texture: " + path + L"\n").c_str());
		return nullptr;
	}

	Texture* raw = texture.get();
	_textures.emplace(key, std::move(texture));
	return raw;
}

Texture* TextureManager::GetWhite()
{
	if (_white) return _white.get();

	// DrawRect 용 1x1 흰 텍스처. "단색 = 흰 텍스처 × 정점 색" 으로 만들어 SpriteBatch 의 셰이더를 하나로 통일한다.
	// 파일이 아니라 메모리에서 만들므로 경로 캐시(_textures)와는 별도로 든다.
	const uint32_t white = 0xFFFFFFFF;
	auto texture = std::make_unique<Texture>();
	if (!texture->CreateFromMemory(_graphics, 1, 1, &white))
	{
		::OutputDebugStringW(L"[TextureManager] failed to create the 1x1 white texture\n");
		return nullptr;
	}
	_white = std::move(texture);
	return _white.get();
}

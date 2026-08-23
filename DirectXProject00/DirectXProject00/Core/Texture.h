#pragma once

// 텍스처 한 장 = GPU 리소스(Texture2D) + 그걸 셰이더에서 읽는 창구(SRV) + 크기. (6단계)
//
// [값 클래스와 관리자를 분리한다]
//   이 클래스는 "텍스처 하나" 라는 값이고, 경로 → Texture 캐시는 TextureManager 가 맡는다.
//   SpriteBatch::Draw() 는 Texture* 만 받고 관리자를 모른다. 이렇게 해야 8단계 아틀라스가
//   "Texture 하나를 공유하는 여러 서브 사각형" 으로 자연스럽게 표현된다.
//
// [Texture2D 와 SRV 를 둘 다 들고 있는 이유]
//   SRV 는 Texture2D 를 내부 참조하므로 SRV 만 들어도 리소스는 살아있다. 하지만 크기/포맷 조회나
//   (후속) 렌더 타겟 텍스처에서 RTV 도 같이 만들어야 할 때 원본 리소스가 필요하다.
//   5단계에서는 SRV 에서 GetResource → GetDesc 로 크기를 역조회했는데, 크기를 여기 들고 있으면 그 조회가 사라진다.
//
// [소유권]
//   TextureManager 가 unique_ptr 로 소유하고 사용자는 raw Texture* 를 들고 있는다. 관리자는 Graphics 다음으로
//   오래 살고 사용자(SpriteBatch 등)는 그보다 먼저 죽는다 — Execute 의 소멸 순서가 보장한다.
class Texture final
{
public:
	Texture() = default;
	~Texture() = default;
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	// WIC 로 파일을 읽어 프리멀티플라이드 RGBA8 로 만든다. 실패 시 false (파일 없음, 포맷 미지원).
	// WIC 팩토리는 TextureManager 가 소유한다 — 수명이 CoUninitialize 보다 앞에 끝나야 하기 때문이다 (TextureManager.h 참고).
	bool LoadFromFile(Graphics* graphics, IWICImagingFactory* wicFactory, const std::wstring& path);
	// 메모리의 RGBA8 픽셀(R,G,B,A 바이트 순서, 프리멀티플라이드)에서 만든다. 1×1 흰 텍스처, 이후 절차적 텍스처용.
	bool CreateFromMemory(Graphics* graphics, uint width, uint height, const void* rgba8Pixels);

	ID3D11ShaderResourceView* GetSRV() const { return _srv.Get(); }
	uint GetWidth()  const { return _width; }
	uint GetHeight() const { return _height; }

private:
	ComPtr<ID3D11Texture2D>          _texture;
	ComPtr<ID3D11ShaderResourceView> _srv;
	uint _width = 0;
	uint _height = 0;
};

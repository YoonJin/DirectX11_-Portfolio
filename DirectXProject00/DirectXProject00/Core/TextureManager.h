#pragma once
#include <memory>
#include <unordered_map>
#include "Texture.h"

// 경로 → Texture 캐시. (6단계)
//
// [관리자는 두 번째 사용자가 생길 때 만든다]
//   5단계까지 텍스처를 쓰는 곳은 SpriteBatch 하나였고 로더도 그 안에 있었다. 8단계(아틀라스)가 두 번째 사용자가
//   되는 것이 확정이라 이 단계에서 추출한다. 관리자가 존재하는 이유는 단 하나 — 같은 파일을 두 번 Load 해도
//   GPU 텍스처는 하나만 만들어진다는 것. 그 외의 새 기능(언로드, 참조 카운트, 핫리로드)은 일부러 넣지 않는다.
//
// [키는 정규화된 절대 경로]
//   L"image/0.png" 와 L"image\\0.png" 와 절대 경로가 모두 같은 텍스처를 가리켜야 한다.
//   std::filesystem::weakly_canonical(AssetRoot() / relativePath) 로 정규화한 뒤 소문자로 통일해 키로 쓴다
//   (Windows 파일 시스템은 대소문자를 구분하지 않는다). weakly_canonical 은 존재하지 않는 파일에도 동작하므로
//   잘못된 경로도 키로는 만들어지고, 로드 단계에서 실패해 nullptr 를 돌려준다.
//
// [소유권]
//   unordered_map<wstring, unique_ptr<Texture>> 가 소유하고 사용자는 raw Texture* 를 들고 있는다.
//   shared_ptr 로 가면 "누가 언제 놓는가" 가 흐려지고, 참조 카운트 기반 언로드는 이 프로젝트 규모에서 필요 없다.
//   전부 Execute 수명이다 — Execute 가 graphics 다음에 만들고 역순으로 지우므로 사용자보다 오래, Graphics 보다 짧게 산다.
//   (unordered_map 은 rehash 로 노드가 이동해도 unique_ptr 가 가리키는 Texture 객체 자체는 움직이지 않으므로 raw 포인터가 유효하다)
//
// [WIC 팩토리를 관리자가 소유하는 이유]
//   Texture.cpp 의 함수-로컬 static ComPtr 로 두면 프로세스 종료 시 CoUninitialize 이후에 파괴될 수 있다
//   (WinMain 은 Execute 를 지운 뒤 CoUninitialize 를 부른다). 관리자 멤버로 두면 수명이 Execute 안에 명확히 갇힌다.
//
// [관리자는 Graphics 에 넣지 않는다]
//   1단계부터의 원칙 — Graphics 는 디바이스·스왑체인·백버퍼·뷰포트까지만. 관리자는 Execute 가 소유하고 Graphics* 를 받아 쓴다.
class TextureManager final
{
public:
	explicit TextureManager(Graphics* graphics);
	~TextureManager();   // 모든 Texture 해제. Graphics 보다 먼저 죽어야 한다 (Execute 소멸 순서)
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	// 경로는 리포 루트 기준 상대 경로 (예: L"image/0.png"). 이미 로드했으면 같은 포인터를 돌려준다.
	// 실패 시 nullptr — 호출자는 null 을 확인해야 한다 (CHECK 는 Release 에서 사라진다). 실패한 경로는 캐시에 남기지 않는다.
	Texture* Load(const std::wstring& relativePath);

	// 1×1 불투명 흰색. 단색 사각형(SpriteBatch::DrawRect)용. 관리자가 처음 요청 시 만들어 소유한다.
	Texture* GetWhite();

private:
	Graphics* _graphics = nullptr;   // 소유하지 않는다 (Execute 가 소유, 이 관리자보다 오래 산다)
	ComPtr<IWICImagingFactory> _wicFactory;
	std::unordered_map<std::wstring, std::unique_ptr<Texture>> _textures;   // 키 = 정규화된 소문자 절대 경로
	std::unique_ptr<Texture> _white;
};

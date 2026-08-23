#pragma once
#include <memory>
#include <unordered_map>
#include "Shader.h"

// 경로(+엔트리) → Shader 캐시. TextureManager 와 같은 꼴이다. (6단계)
//
// [왜 지금 만드는가]
//   셰이더 컴파일 코드가 TestPattern 과 SpriteBatch 에 두 벌 있었다 — "추출할 때가 됐다" 는 신호다.
//   관리자가 존재하려면 소스가 파일이어야 한다(문자열을 키로 캐시할 수는 없다). 그래서 이 단계에서 HLSL 을
//   리포 루트 Shaders/ 폴더로 분리했다. 파일 분리는 새 기능이 아니라 관리자의 전제다.
//
// [키]
//   정규화된 소문자 절대 경로 + "|" + VS 엔트리 + "|" + PS 엔트리. 같은 파일에서 다른 엔트리 쌍을 뽑는 경우를 구분한다.
//   입력 레이아웃은 키에 넣지 않는다 — 입력 레이아웃이 다른 같은 (파일, 엔트리) 조합은 지금 없다.
//   필요해지면 그때 레이아웃 desc 의 해시(Shader::HashLayout)를 키에 붙인다. (지금 넣으면 쓰이지 않는 복잡도다)
//   대신 캐시 적중 시 레이아웃 해시가 다르면 Debug 에서 assert 한다 — "조용히 엉뚱한 레이아웃" 이 되지 않게.
//
// [소유권 / 위치] TextureManager.h 와 같다 — unique_ptr 소유, 사용자는 raw Shader*, Execute 가 관리자를 소유하고 Graphics 에 넣지 않는다.
class ShaderManager final
{
public:
	explicit ShaderManager(Graphics* graphics);
	~ShaderManager();   // 모든 Shader 해제. Graphics 보다 먼저 죽어야 한다 (Execute 소멸 순서)
	ShaderManager(const ShaderManager&) = delete;
	ShaderManager& operator=(const ShaderManager&) = delete;

	// 경로는 리포 루트 기준 상대 경로 (예: L"Shaders/Sprite.hlsl"). 이미 컴파일했으면 같은 포인터를 돌려준다.
	// 컴파일 실패 시 출력창에 원인이 찍히고 CHECK 로 멈춘다 (Debug). Release 에서는 nullptr.
	Shader* Load(const std::wstring& relativePath, const char* vsEntry, const char* psEntry,
	             const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout);

private:
	Graphics* _graphics = nullptr;   // 소유하지 않는다
	std::unordered_map<std::wstring, std::unique_ptr<Shader>> _shaders;
};

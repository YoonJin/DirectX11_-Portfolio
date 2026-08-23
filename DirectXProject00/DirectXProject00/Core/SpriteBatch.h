#pragma once
#include <cstdint>
#include "RenderStates.h"   // BlendMode / SamplerMode

class Texture;
class TextureManager;
struct AtlasFrame;
class Shader;
class ShaderManager;

// 논리 좌표 사각형. (x, y) 는 좌상단, 단위는 Settings 의 논리 해상도 픽셀.
// SpriteBatch 전용 값 구조체라 pch.h 에 넣지 않는다. (Win32 의 RECT 는 left/top/right/bottom 정수라 용도가 다르다)
struct RECT_F
{
	float x, y, width, height;
};

// 정렬 모드 (9단계). End() 에서 큐를 어떤 순서로 flush 할지 정한다 — 정렬과 배칭의 트레이드오프가 여기 있다.
enum class SortMode
{
	Deferred,      // Draw 호출 순서대로. 텍스처가 바뀔 때마다 flush (정렬 없음). 기본값 — 5~8단계와 같은 겹침 순서
	Texture,       // 텍스처별로 묶는다 (텍스처 포인터 값 기준 stable_sort). 드로우콜 최소. 겹침 순서는 보장 안 됨 — 겹치지 않는 타일/파티클용
	BackToFront,   // 뒤(depth 작음)부터 앞(depth 큼) 순. 2D 게임의 y-sort : depth 에 "발 y" 를 넣으면 화면 아래쪽이 나중에 그려져 위쪽을 가린다
	FrontToBack,   // 앞부터 뒤 순. 깊이 버퍼가 있을 때 오버드로우 감소용 — 지금은 깊이 버퍼가 없으므로 BackToFront 의 대칭으로만 둔다
};
// [depth 규약] "클수록 앞(보는 사람에 가깝다)". 2D 에서 가장 흔한 정렬 키가 발 y(아래쪽 = 앞)라서 y 를 그대로 넣을 수 있게 정했다.
//   DirectXTK 의 layerDepth(0 = 앞, 1 = 뒤)와 부호가 반대다 — 이름은 같지만 그쪽 코드를 옮길 때 주의.

// 뒤집기. UV 의 좌/우(상/하)를 바꾼다 — 정점 위치는 그대로라 감기 방향이 바뀌지 않는다 (어차피 7단계부터 CullNone).
enum class SpriteEffects : uint8_t
{
	None = 0,
	FlipHorizontally = 1,
	FlipVertically = 2,
	FlipBoth = 3,
};
inline SpriteEffects operator|(SpriteEffects a, SpriteEffects b) { return static_cast<SpriteEffects>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
inline bool HasEffect(SpriteEffects value, SpriteEffects flag) { return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0; }

// 2D 스프라이트 렌더러 2차 (9단계) — 큐 → 정렬 → flush 배칭.
//
// [5단계 → 9단계 : 즉시 그리기에서 배칭으로]
//   5단계의 Draw() 는 Map + DrawIndexed(6) 을 그 자리에서 했다 (스프라이트 1개 = 드로우콜 1개). 이제 Draw() 는 CPU 배열(_sprites)에 쌓기만 하고,
//   End() 가 정렬한 뒤 "같은 텍스처" 구간을 정점 버퍼 하나에 모아 DrawIndexed 한 번으로 그린다.
//   [드로우콜은 비싸고 정점은 싸다] D3D11 에서 DrawIndexed 한 번의 CPU 비용(드라이버 검증, 커맨드 버퍼 기록)은 정점 수천 개를 채우는 것보다 크다.
//   스프라이트는 정점 4개짜리라 드로우콜 수가 곧 CPU 비용이다. 8단계 아틀라스가 "많은 스프라이트가 같은 텍스처" 라는 전제를 만들었으므로 이제 효과가 난다.
//
// [flush 조건은 텍스처 변경과 용량(kMaxBatchSize) 뿐]
//   블렌드/샘플러는 7단계에서 "한 Begin/End = 한 상태" 로 고정했고 셰이더도 하나다. 그래서 SortMode::Texture 면 텍스처별로 모아 드로우콜을 최소화할 수 있고,
//   BackToFront 면 깊이 순서가 우선이라 텍스처가 바뀔 때마다 끊긴다 — 정렬과 배칭의 트레이드오프를 GetStats() 로 보여 준다.
//
// [Map(WRITE_DISCARD) 는 flush 마다]
//   DISCARD 는 드라이버가 새 메모리를 내어주는 것이므로 flush 마다 불러도 GPU 와 충돌하지 않는다. 한 프레임에 flush 가 수십 번이면 NO_OVERWRITE 로
//   한 버퍼를 이어 쓰는 최적화가 있지만, 측정 전에는 하지 않는다 (09 문서 "하지 않는 것").
//
// [정렬은 stable_sort on 인덱스]
//   SpriteInfo 를 직접 정렬하면 복사가 크다. 인덱스 배열을 정렬한다. stable 인 이유 : 같은 깊이의 스프라이트는 Draw() 호출 순서를 유지해야 한다
//   (같은 y 에 있는 UI 요소들의 겹침 순서가 프레임마다 바뀌면 깜빡인다).
//
// [변환(회전·스케일·뒤집기)은 CPU 에서 정점 4개에 적용]
//   셰이더에 행렬을 넘기는 방식은 스프라이트마다 상수 버퍼 갱신이 필요해 배칭과 상극이다. 정점 4개를 CPU 에서 회전시키는 비용은 무시할 만하다 (DirectXTK 와 같다).
//
// [측정 없는 최적화는 하지 않는다] 이 단계의 규율. 보류한 목록(NO_OVERWRITE, 상태 캐시, 인스턴싱, 멀티스레드)은 09 문서의 "하지 않는 것" 에 있다.
//
// [좌표계 — 투영은 한 번, 뷰포트만 움직인다] (5단계부터 불변)
//   논리 좌표 → 직교 투영(BuildProjection, 논리 해상도 변경 리스너로만 재생성) → NDC → 레터박스 뷰포트(Graphics) → 백버퍼 물리 픽셀.
//   SpriteBatch 는 리사이즈를 모른다. 카메라(뷰 행렬)가 생기면 Begin 에 const XMMATRIX* view 를 받는 오버로드가 자연스러운 자리다 — "논리 좌표 = 화면 좌표" 계약이
//   바뀌는 결정이라 게임 레이어를 설계할 때 한다.
//
// [알파는 프리멀티플라이드] 텍스처는 로드 시 WIC 가, color 인자(straight 0xAABBGGRR)는 Draw() 시점에 PremultiplyColor 가 곱한다. 블렌드 프리셋은 RenderStates.h.
//
// [소유/빌림] 빌리는 것 : Shader*, 흰 Texture*, RenderStates*, Draw() 인자의 Texture* (관리자 소유, 이 클래스보다 오래 산다). 소유하는 것 : VB(동적, kMaxBatchSize×4) / IB(불변, ×6) / 투영 상수 버퍼.
class SpriteBatch final
{
public:
	// 배치 하나의 최대 스프라이트 수. 정점 버퍼 = kMaxBatchSize × 4 정점(20B) = 160KB. 넘치면 flush 하고 이어 간다 (flushesByCapacity).
	static constexpr uint kMaxBatchSize = 2048;

	// 통계. drawCalls = DrawIndexed 횟수. flushesByTexture / flushesByCapacity 는 "추가" 드로우콜의 원인 분해 —
	// 한 Begin/End 의 마지막 flush 는 어느 쪽도 아니므로 drawCalls = (Begin/End 수) + flushesByTexture + flushesByCapacity.
	struct Stats
	{
		uint drawCalls = 0;
		uint sprites = 0;
		uint flushesByTexture = 0;
		uint flushesByCapacity = 0;
		uint batches = 0;   // Begin/End 쌍 수
	};

	SpriteBatch(Graphics* graphics, TextureManager* textureManager, ShaderManager* shaderManager, RenderStates* renderStates);
	~SpriteBatch();

	// 호출 규약 : Begin(sort, blend, sampler) → Draw*() 여러 번 → End(). Begin 없이 Draw 를 부르면 assert.
	// 한 Begin/End = 한 블렌드/샘플러 상태 (7단계 규약). 정렬 모드도 마찬가지로 Begin 에서 고정된다.
	void Begin(SortMode sort = SortMode::Deferred, BlendMode blend = BlendMode::AlphaBlend, SamplerMode sampler = SamplerMode::PointClamp);

	// 텍스처 스프라이트 (전체 인자).
	//   texture  : TextureManager::Load() 로 받은 Texture. null 이면 그리지 않는다
	//   dst      : 논리 좌표 사각형 (회전 전). 스케일은 dst 의 크기로 표현한다 (텍셀 크기 × 배율)
	//   src      : 텍셀 사각형. nullptr 이면 전체. Draw() 시점에 UV 로 바꿔 저장한다 (flush 에서 텍스처 크기 조회를 피한다)
	//   color    : 틴트. straight 알파 0xAABBGGRR
	//   rotation : 라디안, 시계 방향(y 가 아래로 가는 좌표계라 양의 각이 시계 방향). origin 기준
	//   origin   : dst 안의 회전 중심. dst 좌상단 기준 논리 단위 (예: { dst.width/2, dst.height/2 } = 제자리 회전)
	//   depth    : 정렬 키 (BackToFront / FrontToBack). 클수록 앞. y-sort 는 "발 y" 를 넣는다. Deferred / Texture 에서는 무시
	//   effects  : 뒤집기
	void Draw(const Texture* texture, const RECT_F& dst, const RECT_F* src, uint32_t color,
	          float rotation, XMFLOAT2 origin, float depth, SpriteEffects effects = SpriteEffects::None);

	// 5단계 호환 : 회전 0, origin (0,0), depth 0.
	void Draw(const Texture* texture, const RECT_F& dst, const RECT_F* src, uint32_t color)
	{
		Draw(texture, dst, src, color, 0.f, { 0.f, 0.f }, 0.f, SpriteEffects::None);
	}

	// 아틀라스 프레임을 피벗 기준으로 그린다 (8단계 + 9단계 확장). position 은 피벗이 놓일 논리 좌표, scale 은 배율.
	// 피벗이 곧 회전 중심(origin)이다 — 발을 중심으로 돈다. depth 를 생략하면 position.y (= 발 y) 가 들어가 y-sort 가 바로 된다.
	void Draw(const Texture* texture, const AtlasFrame& frame, XMFLOAT2 position, float scale = 1.f, uint32_t color = 0xFFFFFFFF,
	          float rotation = 0.f, SpriteEffects effects = SpriteEffects::None, const float* depth = nullptr);

	// 단색 사각형. TextureManager 의 1x1 흰 텍스처로 Draw() 에 위임한다 — 셰이더를 하나로 통일하기 위한 표준 기법.
	// (배칭의 "텍스처 변경 = flush" 규칙이 단색 사각형에도 똑같이 적용된다 — 흰 텍스처도 텍스처다)
	void DrawRect(const RECT_F& dst, uint32_t color);

	// 큐를 정렬하고 flush 한다. 여기서만 GPU 를 건드린다.
	void End();

	// 직전 End() 의 통계 (한 Begin/End 분).
	const Stats& GetStats() const { return _stats; }
	// 프레임 누적 통계 : ResetFrameStats() 이후의 모든 Begin/End 합. Execute 가 프레임 시작에 리셋하고 제목에 쓴다 —
	// CPU 프레임 시간(프레임 전체)과 같은 범위의 숫자여야 비교가 된다.
	const Stats& GetFrameStats() const { return _frameStats; }
	void ResetFrameStats() { _frameStats = Stats{}; }

private:
	// 정점 포맷 (20 바이트). 입력 레이아웃의 DXGI_FORMAT_R8G8B8A8_UNORM 이 color 를 float4(0..1) 로 자동 정규화한다.
	struct SpriteVertex
	{
		XMFLOAT2 position;   // 논리 좌표
		XMFLOAT2 uv;         // 0..1
		uint32_t color;      // 프리멀티플라이드 RGBA8 (메모리 순서 R,G,B,A)
	};
	static_assert(sizeof(SpriteVertex) == 20, "SpriteVertex layout must match the input layout");

	// 큐의 원소. Draw() 가 채우고 End() 가 읽는다. 정점 생성에 필요한 것만 — 텍스처 크기 조회나 프리멀티플라이는 Draw() 시점에 끝낸다.
	struct SpriteInfo
	{
		const Texture* texture;
		RECT_F         dst;        // 논리 좌표 (회전 전)
		RECT_F         srcUV;      // x, y, width, height 가 전부 0..1 UV (뒤집기는 flush 에서 적용)
		uint32_t       color;      // 프리멀티플라이드 완료
		float          rotation;
		XMFLOAT2       origin;
		float          depth;
		SpriteEffects  effects;
	};

	void CreateBuffers();        // 정점(동적, kMaxBatchSize×4) / 인덱스(불변, kMaxBatchSize×6) / 상수(투영)
	void BuildProjection();      // Settings 논리 해상도 → 직교 행렬 → 상수 버퍼. 생성 시 + 논리 해상도 변경 시
	void SortSprites();          // _sortedIndices 를 SortMode 에 따라 정렬
	void Flush(const size_t* indices, size_t count);   // 같은 텍스처 구간 하나 → Map, 정점 생성, Unmap, DrawIndexed
	static void MakeVertices(const SpriteInfo& sprite, SpriteVertex* out);   // 회전/뒤집기 적용해 정점 4개

	Graphics* _graphics = nullptr;   // 소유하지 않는다 (Execute 가 소유, SpriteBatch 보다 오래 산다)
	uint _logicalResolutionListenerId = 0;

	// ---- 빌리는 것 (관리자 소유) ----
	Shader*        _shader = nullptr;         // Shaders/Sprite.hlsl VS/PS + 입력 레이아웃
	const Texture* _whiteTexture = nullptr;   // 1x1 흰색 (DrawRect)
	RenderStates*  _renderStates = nullptr;   // 블렌드/샘플러/래스터 프리셋

	// ---- 소유하는 것 ----
	ComPtr<ID3D11Buffer> _vertexBuffer;     // D3D11_USAGE_DYNAMIC, kMaxBatchSize 쿼드분. flush 마다 Map(WRITE_DISCARD)
	ComPtr<ID3D11Buffer> _indexBuffer;      // D3D11_USAGE_IMMUTABLE, (0 1 2 0 2 3) + 4k 패턴 반복
	ComPtr<ID3D11Buffer> _constantBuffer;   // XMFLOAT4X4 projection (64 바이트)

	// ---- 큐 ----
	std::vector<SpriteInfo> _sprites;        // Begin 에서 clear, Draw 에서 push, End 에서 flush. capacity 는 프레임 간 유지된다
	std::vector<size_t>     _sortedIndices;  // 정렬 결과 (SpriteInfo 복사를 피한다)
	SortMode _sortMode = SortMode::Deferred;
	Stats    _stats;        // 직전 Begin/End
	Stats    _frameStats;   // ResetFrameStats 이후 누적
	bool     _inBeginEnd = false;
};

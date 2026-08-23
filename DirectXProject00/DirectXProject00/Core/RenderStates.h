#pragma once
#include "BlendState.h"
#include "SamplerState.h"
#include "RasterizerState.h"

// 블렌드 프리셋.
//   Opaque     : 블렌드 끔. 알파 무시 — 불투명 배경, TestPattern. 프리멀티플라이드 텍스처를 이걸로 그리면 투명 영역은 RGB 가 (0,0,0) 이라 "검정" 이 된다
//                (straight 알파였다면 투명 영역에 뭐가 들어 있었는지에 따라 쓰레기 색이 나온다 — 프리멀티플라이드를 고른 이유의 시각적 증거).
//   AlphaBlend : ONE / INV_SRC_ALPHA. 프리멀티플라이드 일반 합성 (5단계와 동일). result = src.rgb + dst.rgb * (1 - src.a)
//   Additive   : ONE / ONE. 가산 — 빛/이펙트. 텍스처 알파를 무시하고 무조건 더한다.
//                [프리멀티플라이드가 가산을 공짜로 만든다] AlphaBlend 에서도 텍스처 알파가 0 이면 dst * 1 + src = 가산이다. 즉 이펙트 텍스처를
//                알파 0 으로 저장하면 별도 상태 없이 가산이 된다. 그래도 Additive 를 따로 두는 이유는 "일반 텍스처(알파 1)를 가산으로 그리고 싶을 때" 다.
enum class BlendMode { Opaque, AlphaBlend, Additive, Count };

// 샘플러 프리셋. 필터(포인트 = 각진 픽셀 / 선형 = 뭉개짐) × 주소 모드(클램프 = 가장자리 고정 / 랩 = 타일링).
enum class SamplerMode { PointClamp, LinearClamp, PointWrap, LinearWrap, Count };

// 래스터라이저 프리셋. 2D 는 컬링이 필요 없다 — 9단계의 회전/뒤집기(스케일 -1)로 정점 순서가 뒤집혀도 사라지지 않아야 한다. 3D 가 생기면 CullBack 추가.
enum class RasterMode { CullNone, Count };

// 렌더 상태 프리셋 관리자 (7단계). 생성자에서 전부 만들어 두고 Bind* 로 바인딩만 한다.
//
// [왜 enum 프리셋인가]
//   상태 객체는 불변·고비용이라 미리 만들어 두는 것이 D3D11 의 의도다 (BlendState.h). 자유 desc 를 키로 캐시하는 일반형(해시)도 가능하지만
//   2D 게임이 실제로 쓰는 조합은 손에 꼽으므로 enum 이 호출부에서 읽기 쉽다 (BlendMode::Additive). 조합이 늘면 열거형에 한 줄 + 생성자에 desc 한 줄.
//   DirectXTK CommonStates 와 같은 접근이다.
//
// [되돌리기는 없다 — 모든 드로우 주체가 자기 상태를 명시한다]
//   5·6단계의 SpriteBatch::End() 는 OMSetBlendState(nullptr) 로 "되돌렸다". 그리는 주체가 늘면 "무엇으로 되돌리는가" 가 모호해진다.
//   대신 모든 드로우 주체(TestPattern::Draw, SpriteBatch::Begin)가 자기 블렌드·래스터·(필요 시) 샘플러를 매번 명시적으로 건다.
//   이러면 바인딩 순서에 의존하는 암묵적 상태가 없어지고, 어떤 주체를 빼거나 순서를 바꿔도 화면이 같다 (검증 7).
//
// [중복 바인딩 회피는 지금 하지 않는다]
//   "마지막으로 바인딩한 상태" 를 기억해 같은 상태면 API 호출을 건너뛰는 최적화는 9단계(배칭)에서 드로우콜이 늘어났을 때 측정하고 넣는다.
//   지금 넣으면 측정 없는 최적화다.
//
// [Release 에서 Create 실패 시]
//   CHECK 가 사라지므로 실패한 상태는 null 로 남고, Bind* 는 null 을 그대로 건다 — D3D 에서 null 상태 바인딩은 합법이고 "디폴트" 로 돌아간다 :
//   블렌드 null = 불투명(AlphaBlend/Additive 스프라이트가 검정 사각형으로), 래스터 null = CullBack(9단계 뒤집기가 사라짐), 샘플러 null = LINEAR/CLAMP(포인트 요청이 선형으로).
//   크래시도 D3D 에러도 없이 조용히 틀리므로, 생성자가 실패를 출력창에 한 번 찍는다 (TextureManager/Shader 와 같은 규칙).
//
// [소유] Execute 가 소유 (graphics → textureManager → shaderManager → renderStates → testPattern → spriteBatch). Graphics 에 넣지 않는다.
class RenderStates final
{
public:
	explicit RenderStates(Graphics* graphics);
	~RenderStates() = default;
	RenderStates(const RenderStates&) = delete;
	RenderStates& operator=(const RenderStates&) = delete;

	void BindBlend(BlendMode mode);                       // OMSetBlendState(state, nullptr, 0xFFFFFFFF)
	void BindSampler(SamplerMode mode, uint slot = 0);    // PSSetSamplers(slot, 1, ...)
	void BindRasterizer(RasterMode mode);                 // RSSetState

private:
	Graphics* _graphics = nullptr;   // 소유하지 않는다
	BlendState      _blend[static_cast<size_t>(BlendMode::Count)];
	SamplerState    _sampler[static_cast<size_t>(SamplerMode::Count)];
	RasterizerState _raster[static_cast<size_t>(RasterMode::Count)];
};

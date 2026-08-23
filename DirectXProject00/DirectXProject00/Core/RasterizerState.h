#pragma once

// 래스터라이저 상태 값 클래스. BlendState / SamplerState 와 같은 꼴. (7단계)
//
// [지금까지는 프로젝트 어디에도 없었다]
//   6단계까지는 디폴트(RSSetState(nullptr) = CullBack, 솔리드)에 의존했다. 쿼드와 풀스크린 삼각형의 정점 순서가
//   시계 방향이라 컬링에 안 걸렸을 뿐이다. 7단계의 "모든 드로우 주체는 자기 상태를 명시한다" 규칙에 따라 이 상태도 명시한다.
class RasterizerState final
{
public:
	RasterizerState() = default;
	~RasterizerState() = default;
	RasterizerState(const RasterizerState&) = delete;
	RasterizerState& operator=(const RasterizerState&) = delete;

	bool Create(Graphics* graphics, const D3D11_RASTERIZER_DESC& desc);
	ID3D11RasterizerState* Get() const { return _state.Get(); }

private:
	ComPtr<ID3D11RasterizerState> _state;
};

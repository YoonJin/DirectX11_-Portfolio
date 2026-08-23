#pragma once

// 블렌드 상태 값 클래스. SamplerState 와 같은 꼴 — Create(desc) + Get(). (7단계)
//
// [상태 객체는 불변이고 비싸다]
//   ID3D11BlendState 는 만들 때 드라이버가 desc 를 검증하고, 한 번 만들면 내용을 바꿀 수 없다. 매 프레임 CreateBlendState 를
//   부르면 안 되고, 필요한 조합을 미리 만들어 두고 OMSetBlendState 로 "바인딩만" 바꾸는 것이 D3D11 의 설계 의도다.
//   그래서 이 클래스는 desc 를 받아 한 번 만들고, 프리셋 묶음(RenderStates)이 생성자에서 전부 만들어 둔다.
class BlendState final
{
public:
	BlendState() = default;
	~BlendState() = default;
	BlendState(const BlendState&) = delete;
	BlendState& operator=(const BlendState&) = delete;

	bool Create(Graphics* graphics, const D3D11_BLEND_DESC& desc);
	ID3D11BlendState* Get() const { return _state.Get(); }

private:
	ComPtr<ID3D11BlendState> _state;
};

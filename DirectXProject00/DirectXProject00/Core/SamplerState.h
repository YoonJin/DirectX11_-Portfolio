#pragma once

// 샘플러 = "텍스처를 어떻게 읽을 것인가" (필터, 주소 모드). (6단계)
//
// 텍스처와 독립이라 따로 둔다 — 같은 텍스처를 포인트로도 선형으로도 읽을 수 있다. 그래서 Texture 의 멤버가 아니다.
//
// [관리자는 만들지 않는다]
//   샘플러는 종류가 손에 꼽는다 (포인트/선형 × 클램프/랩). 7단계 RenderStates 가 블렌드/래스터와 함께
//   "프리셋 열거형 → 상태 객체" 로 묶는다. 이 단계는 클래스만 뽑아 SpriteBatch 가 SamplerState 멤버를 갖게 하는 데서 멈춘다.
//   (7단계의 블렌드/래스터 상태 클래스도 이 꼴 — Create(desc 인자) + Get() — 을 따른다)
class SamplerState final
{
public:
	SamplerState() = default;
	~SamplerState() = default;
	SamplerState(const SamplerState&) = delete;
	SamplerState& operator=(const SamplerState&) = delete;

	// address 는 U/V/W 에 같이 적용한다 (2D 라 W 는 쓰이지 않지만 desc 는 채워야 한다).
	bool Create(Graphics* graphics, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE address);

	ID3D11SamplerState* Get() const { return _state.Get(); }
	// PSSetSamplers 같은 "ID3D11SamplerState* const*" 인자용. GetAddressOf 라 기존 객체를 해제하지 않는다.
	ID3D11SamplerState* const* GetAddressOf() const { return _state.GetAddressOf(); }

private:
	ComPtr<ID3D11SamplerState> _state;
};

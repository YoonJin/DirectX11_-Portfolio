#include "pch.h"
#include "SamplerState.h"

bool SamplerState::Create(Graphics* graphics, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE address)
{
	// 5단계 SpriteBatch::CreateStates() 의 샘플러 desc 를 그대로 옮긴 것이다. 필터/주소만 인자로 뺐다.
	// ComparisonFunc NEVER : 비교 샘플러(그림자 맵용)가 아니다. MinLOD 0 / MaxLOD FLOAT32_MAX : 밉 범위 제한 없음 (밉맵 자체가 없다).
	if (!graphics) return false;
	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = filter;
	desc.AddressU = address;
	desc.AddressV = address;
	desc.AddressW = address;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MinLOD = 0.f;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	HRESULT hr = graphics->GetDevice()->CreateSamplerState(&desc, _state.ReleaseAndGetAddressOf());
	CHECK(hr);
	return SUCCEEDED(hr);
}

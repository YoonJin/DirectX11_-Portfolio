#include "pch.h"
#include "BlendState.h"

bool BlendState::Create(Graphics* graphics, const D3D11_BLEND_DESC& desc)
{
	if (!graphics) return false;
	HRESULT hr = graphics->GetDevice()->CreateBlendState(&desc, _state.ReleaseAndGetAddressOf());
	CHECK(hr);
	return SUCCEEDED(hr);
}

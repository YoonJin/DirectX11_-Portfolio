#include "pch.h"
#include "RasterizerState.h"

bool RasterizerState::Create(Graphics* graphics, const D3D11_RASTERIZER_DESC& desc)
{
	if (!graphics) return false;
	HRESULT hr = graphics->GetDevice()->CreateRasterizerState(&desc, _state.ReleaseAndGetAddressOf());
	CHECK(hr);
	return SUCCEEDED(hr);
}

#include "pch.h"
#include "RenderStates.h"

namespace
{
	// 블렌드 desc 공통부. 알파 채널도 같은 식으로 누적한다 — 백버퍼 알파는 Present 에 쓰이지 않지만, 나중에 오프스크린 RT 에 그릴 때 올바른 값이 남는다.
	D3D11_BLEND_DESC MakeBlendDesc(BOOL enable, D3D11_BLEND src, D3D11_BLEND dest)
	{
		D3D11_BLEND_DESC desc = {};
		D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];   // IndependentBlendEnable = FALSE 라 [0] 이 모든 RT 에 적용된다
		rt.BlendEnable = enable;
		rt.SrcBlend = src;
		rt.DestBlend = dest;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = src;
		rt.DestBlendAlpha = dest;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}
}

RenderStates::RenderStates(Graphics* graphics)
	: _graphics(graphics)
{
	bool ok = true;   // Release 에서 CHECK 가 사라졌을 때 실패를 한 번은 알리기 위해 모은다 (RenderStates.h 의 "Release 에서 Create 실패 시")

	// ---- 블렌드 (RenderStates.h 의 BlendMode 주석 참고) ----
	// Opaque : BlendEnable = FALSE. Src/Dest 값은 무시되지만 디버그 레이어가 "유효하지 않은 값" 을 보지 않도록 ONE/ZERO 를 채운다.
	ok &= _blend[static_cast<size_t>(BlendMode::Opaque)].Create(_graphics, MakeBlendDesc(FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO));
	// AlphaBlend : src.rgb 에는 이미 알파가 곱해져 있으므로 SrcBlend 는 ONE (straight 알파였다면 SRC_ALPHA 였을 자리). 5·6단계 SpriteBatch 의 상태와 동일.
	ok &= _blend[static_cast<size_t>(BlendMode::AlphaBlend)].Create(_graphics, MakeBlendDesc(TRUE, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA));
	// Additive : ONE / ONE. 알파 채널도 ONE / ONE 이라 백버퍼 알파가 1 을 넘어 포화될 뿐, 화면에는 영향 없다.
	ok &= _blend[static_cast<size_t>(BlendMode::Additive)].Create(_graphics, MakeBlendDesc(TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE));

	// ---- 샘플러 ----
	// 포인트 : 2D 픽셀 스프라이트의 기본. 레터박스 scale 이 정수가 아니면 약간 흐려지지만 픽셀 경계가 유지된다 (4단계 Letterbox(integerScale) 와 짝).
	// 선형  : 축소/확대 시 부드럽다. 1/2 이하로 줄면 밉맵 없이는 깜빡인다 ("하지 않는 것" 참고).
	// 클램프 : 가장자리 바깥을 샘플할 때 반대편 픽셀이 새어 들어오지 않는다 (아틀라스에서 중요). 랩 : 타일링 배경용.
	ok &= _sampler[static_cast<size_t>(SamplerMode::PointClamp)].Create(_graphics, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	ok &= _sampler[static_cast<size_t>(SamplerMode::LinearClamp)].Create(_graphics, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	ok &= _sampler[static_cast<size_t>(SamplerMode::PointWrap)].Create(_graphics, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP);
	ok &= _sampler[static_cast<size_t>(SamplerMode::LinearWrap)].Create(_graphics, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);

	// ---- 래스터라이저 ----
	// CullMode NONE : 2D 에서는 뒷면 컬링으로 얻는 게 없고, 9단계의 뒤집기(스케일 -1)가 정점 순서를 뒤집어도 사라지지 않아야 한다.
	// DepthClipEnable TRUE : 디폴트와 같다 (NDC z 0..1 밖은 잘라냄. 우리 z 는 항상 0). ScissorEnable FALSE : UI 클리핑은 후속.
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthClipEnable = TRUE;
		desc.ScissorEnable = FALSE;
		ok &= _raster[static_cast<size_t>(RasterMode::CullNone)].Create(_graphics, desc);
	}

	if (!ok) ::OutputDebugStringA("[RenderStates] state creation failed; null (D3D default) states will be bound for the failed presets\n");
}

void RenderStates::BindBlend(BlendMode mode)
{
	assert(static_cast<size_t>(mode) < static_cast<size_t>(BlendMode::Count));   // unsigned 비교 — 음수 캐스트도 걸린다
	// 블렌드 팩터는 BLEND_FACTOR 모드에서만 쓰므로 nullptr. 샘플 마스크 0xFFFFFFFF = 모든 샘플에 쓴다.
	_graphics->GetDeviceContext()->OMSetBlendState(_blend[static_cast<size_t>(mode)].Get(), nullptr, 0xFFFFFFFF);
}

void RenderStates::BindSampler(SamplerMode mode, uint slot)
{
	assert(static_cast<size_t>(mode) < static_cast<size_t>(SamplerMode::Count));
	_graphics->GetDeviceContext()->PSSetSamplers(slot, 1, _sampler[static_cast<size_t>(mode)].GetAddressOf());
}

void RenderStates::BindRasterizer(RasterMode mode)
{
	assert(static_cast<size_t>(mode) < static_cast<size_t>(RasterMode::Count));
	_graphics->GetDeviceContext()->RSSetState(_raster[static_cast<size_t>(mode)].Get());
}

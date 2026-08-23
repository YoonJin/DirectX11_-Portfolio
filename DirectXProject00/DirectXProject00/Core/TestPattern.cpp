#include "pch.h"
#include "TestPattern.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "RenderStates.h"

TestPattern::TestPattern(Graphics* graphics, ShaderManager* shaderManager, RenderStates* renderStates)
	: _graphics(graphics), _renderStates(renderStates)
{
	ID3D11Device* device = _graphics->GetDevice();

	// [HLSL 은 Shaders/TestPattern.hlsl]
	// 1.5단계에서는 소스가 이 파일 안의 문자열이었고 D3DCompile 코드도 여기 있었다. SpriteBatch 가 같은 코드를 한 벌 더 갖게 되자
	// 6단계에서 Shader/ShaderManager 로 추출했다 — 컴파일 규칙(vs_4_0/ps_4_0, 디버그 플래그, 에러 출력)은 그쪽 한 곳에만 있다.
	// 입력 레이아웃은 빈 벡터 : 정점 버퍼 없이 SV_VertexID 만 쓴다.
	// (Release 에서 컴파일이 실패하면 null — Draw() 는 null 셰이더를 바인딩하고 아무것도 그리지 않을 뿐 죽지는 않는다)
	_shader = shaderManager->Load(L"Shaders/TestPattern.hlsl", "VS", "PS", {});
	assert(_shader && "TestPattern: Shaders/TestPattern.hlsl failed to compile (see debug output)");
	HRESULT hr = S_OK;

	// 상수 버퍼 : 매 프레임 CPU 가 써넣고 GPU 가 읽는 작은 버퍼.
	// USAGE_DEFAULT + UpdateSubresource 로 갱신한다. (DYNAMIC + Map 은 후속 작업에서 대량 갱신 때 쓴다)
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(PatternConstants);   // 16 의 배수여야 한다 (헤더의 static_assert 가 보장)
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&desc, nullptr, _constantBuffer.GetAddressOf());
	CHECK(hr);
}

void TestPattern::Draw()
{
	ID3D11DeviceContext* context = _graphics->GetDeviceContext();

	// 현재 뷰포트를 셰이더에 넘긴다. Resize 를 구독할 필요가 없다 — 매 프레임 읽으면 된다.
	const D3D11_Viewport& vp = _graphics->GetViewport();
	PatternConstants constants = { vp.x, vp.y, vp.width, vp.height };
	context->UpdateSubresource(_constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

	if (!_shader) return;                                                 // Release 에서 컴파일 실패 — 이전 드로우의 셰이더로 그리지 않는다

	// 자기 상태를 명시한다 (7단계 규칙). 6단계까지는 디폴트(nullptr) 와 SpriteBatch::End() 의 "되돌리기" 에 의존했다.
	// 이제 이 드로우는 앞에 무엇이 그려졌든 같은 결과를 낸다. 샘플러는 쓰지 않으므로 바인딩하지 않는다.
	_renderStates->BindBlend(BlendMode::Opaque);
	_renderStates->BindRasterizer(RasterMode::CullNone);
	_shader->Bind(context);                                               // 입력 레이아웃 null(정점 입력 없음, SV_VertexID 만 사용) + VS + PS
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());  // register(b0)

	context->Draw(3, 0);   // 정점 3개 = 풀스크린 삼각형 하나
}

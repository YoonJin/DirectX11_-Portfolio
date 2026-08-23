#pragma once

class Shader;
class ShaderManager;
class RenderStates;

// 리사이즈 / 레터박스 검증용 절차적 테스트 패턴.
// 정점 버퍼 없이 풀스크린 삼각형 하나를 그리고, 픽셀 셰이더가 뷰포트 기준 좌표로 패턴을 계산한다.
//   - 1픽셀 체커보드 : 백버퍼가 창 크기와 어긋나면(Blt 스케일링) 회색으로 뭉개진다 → 2단계(창 리사이즈) 검증
//   - 중앙 링         : 종횡비 왜곡 시 타원이 된다                                   → 4단계(논리 해상도) 검증
//   - 십자선 + 테두리 : 뷰포트 원점과 경계                                           → 4단계(레터박스) 검증
//
// [왜 텍스처나 삼각형이 아니라 절차적 패턴인가]
//   텍스처는 로더·SRV·샘플러가 필요해 후속 작업을 앞당기게 되고, 큰 도형 하나는 살짝 흐려져도 눈에 잘 띄지 않는다.
//   절차적 체커보드는 해상도 독립적이다 — 어떤 크기에서도 "1픽셀 격자"여야 하므로 흐려지면 무조건 문제다.
//   "기능보다 검증 수단을 먼저 만든다"는 접근이 이 클래스의 존재 이유다.
//
// 셰이더는 리포 루트의 Shaders/TestPattern.hlsl 이고 ShaderManager 에서 빌린다 (6단계. 1.5단계에서는 .cpp 안의 문자열이었다).
class TestPattern final
{
public:
	TestPattern(Graphics* graphics, ShaderManager* shaderManager, RenderStates* renderStates);
	~TestPattern() = default;

	void Draw();

private:
	// 셰이더에 넘기는 값. 상수 버퍼는 크기가 16 바이트 배수여야 한다 (float4 하나 = 16 바이트).
	struct PatternConstants
	{
		float viewportX, viewportY, viewportWidth, viewportHeight;
	};
	static_assert(sizeof(PatternConstants) % 16 == 0, "constant buffer size must be a multiple of 16 bytes");

	Graphics* _graphics = nullptr;   // 소유하지 않는다 (Execute 가 소유)

	Shader* _shader = nullptr;       // ShaderManager 소유 (빌린 것). VS/PS 만 있고 입력 레이아웃은 없다 (SV_VertexID 만 사용)
	RenderStates* _renderStates = nullptr;   // Draw() 가 Opaque / CullNone 을 명시하는 데 쓴다 (7단계)
	ComPtr<ID3D11Buffer> _constantBuffer;
};

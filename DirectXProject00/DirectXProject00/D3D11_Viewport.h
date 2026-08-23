#pragma once

class D3D11_Viewport final
{
public:
	D3D11_Viewport
	(
		const float& x = 0.0f,
		const float& y = 0.0f,
		const float& width = 0.0f,
		const float& height = 0.0f,
		const float& min_depth = 0.0f,
		const float& max_depth = 1.0f
	);
	D3D11_Viewport(const D3D11_Viewport& rhs);
	~D3D11_Viewport() = default;

	bool operator==(const D3D11_Viewport& rhs) const
	{
		return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height && min_depth == rhs.min_depth && max_depth == rhs.max_depth;
	}
	bool operator!=(const D3D11_Viewport& rhs) const
	{
		return !(*this == rhs);
	}

	bool IsDefined() const
	{
		return x != 0.0f || y != 0.0f || width != 0.0f || height != 0.0f || min_depth != 0.0f || max_depth != 0.0f;
	}
	auto AspectRatio() const { return width / height; }

	// ---- 레터박스 (4단계) ----
	// 논리 해상도의 종횡비를 유지하면서 백버퍼 안에 최대 크기로 중앙 정렬되는 뷰포트를 만든다.
	//   scale = min(물리W/논리W, 물리H/논리H)  -> 가로/세로 중 더 빡빡한 쪽에 맞춘다
	//   뷰포트 크기 = 논리 크기 * scale
	//   남는 공간을 반으로 나눠 offset 으로 쓴다 -> 중앙 정렬
	// 창이 논리보다 가로로 넓으면 좌우에 빈 공간(필러박스), 세로로 길면 위아래에 빈 공간(레터박스)이 생긴다.
	// [왜 뷰포트로 하는가] 종횡비를 유지하지 않고 늘리면 원형 스프라이트가 타원이 된다.
	//   뷰포트 변환은 NDC(-1..1) -> 픽셀 매핑의 마지막 단계라, 여기서 종횡비를 고정하면 투영 행렬은 손댈 필요가 없다.
	// 뷰포트 크기가 논리 해상도의 정수배가 아니면 스프라이트가 비정수 배율로 샘플링되어 약간 흐려질 수 있다.
	// 픽셀 아트 게임은 scale 을 floor 해서 정수배만 쓰기도 한다 (integerScale 옵션. 1 배 미만이면 적용하지 않는다).
	// (이 헤더는 pch.h 를 include 하지 않으므로 typedef uint 에 의존하지 않고 unsigned int 로 쓴다)
	static D3D11_Viewport Letterbox(
		const unsigned int& backBufferWidth, const unsigned int& backBufferHeight,
		const unsigned int& logicalWidth,    const unsigned int& logicalHeight,
		bool integerScale = false);

	// ---- 물리 <-> 논리 좌표 변환 ----
	// 마우스 입력은 물리 픽셀(클라이언트 좌표)로 들어오는데 게임은 논리 좌표를 쓴다.
	// 변환은 "뷰포트가 어디에 얼마 크기로 놓였는가" 를 아는 쪽이 제공해야 하므로 여기에 둔다.
	// Letterbox() 로 만든 뷰포트는 가로/세로 scale 이 같으므로 width / logicalWidth 하나로 충분하다.
	// 뷰포트가 아직 정해지지 않았거나(width 0) 논리 폭이 0 이면 0 으로 나누게 되므로 scale 1 로 대체한다 (inf/NaN 전파 방지).

	// 물리(백버퍼) 좌표 -> 논리 좌표. 바 영역을 가리키면 논리 범위 밖의 값(음수 또는 논리 크기 초과)이 나온다. 판단은 호출자가 한다.
	void PhysicalToLogical(float px, float py, const unsigned int& logicalWidth, float& outX, float& outY) const
	{
		const float scale = (width > 0.f && logicalWidth > 0) ? width / logicalWidth : 1.f;
		outX = (px - x) / scale;
		outY = (py - y) / scale;
	}
	// 논리 좌표 -> 물리(백버퍼) 좌표. UI 디버그 표시 등에 쓴다.
	void LogicalToPhysical(float lx, float ly, const unsigned int& logicalWidth, float& outX, float& outY) const
	{
		const float scale = (width > 0.f && logicalWidth > 0) ? width / logicalWidth : 1.f;
		outX = x + lx * scale;
		outY = y + ly * scale;
	}

public:
	static const D3D11_Viewport Undefined_viewport;

public:
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	float min_depth = 0.0f;
	float max_depth = 0.0f;
};
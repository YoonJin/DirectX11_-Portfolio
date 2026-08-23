#include "pch.h"
#include "D3D11_Viewport.h"
#include <cmath>   // floorf. pch 를 거쳐 간접 포함되지만 의존을 명시한다.

const D3D11_Viewport D3D11_Viewport::Undefined_viewport(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

D3D11_Viewport::D3D11_Viewport(const float& x, const float& y, const float& width, const float& height, const float& min_depth, const float& max_depth)
	: x(x)
	, y(y)
	, width(width)
	, height(height)
	, min_depth(min_depth)
	, max_depth(max_depth)
{
}

D3D11_Viewport::D3D11_Viewport(const D3D11_Viewport& rhs)
	: x(rhs.x)
	, y(rhs.y)
	, width(rhs.width)
	, height(rhs.height)
	, min_depth(rhs.min_depth)
	, max_depth(rhs.max_depth)
{

}

D3D11_Viewport D3D11_Viewport::Letterbox(
	const unsigned int& backBufferWidth, const unsigned int& backBufferHeight,
	const unsigned int& logicalWidth,    const unsigned int& logicalHeight,
	bool integerScale)
{
	// 논리 크기가 0 이면 나눗셈이 불가능하다. 호출자의 설정 실수이므로 디버그에서 잡고, Release 에서는 전체 뷰포트로 대체한다.
	assert(logicalWidth > 0 && logicalHeight > 0);
	if (logicalWidth == 0 || logicalHeight == 0)
		return D3D11_Viewport(0.f, 0.f, static_cast<float>(backBufferWidth), static_cast<float>(backBufferHeight), 0.f, 1.f);

	// 가로/세로 각각 "백버퍼에 꽉 채우려면 몇 배여야 하는가" 를 구하고, 둘 중 작은 쪽을 고른다.
	// 큰 쪽을 고르면 한 축이 백버퍼 밖으로 잘려 나간다.
	const float scaleX = static_cast<float>(backBufferWidth) / logicalWidth;
	const float scaleY = static_cast<float>(backBufferHeight) / logicalHeight;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;
	if (integerScale && scale >= 1.f) scale = floorf(scale);

	const float viewportWidth = logicalWidth * scale;
	const float viewportHeight = logicalHeight * scale;
	// 남는 공간을 반씩 나눠 중앙에 놓는다. 종횡비가 같은 축은 남는 공간이 0 이라 offset 도 0 이 된다.
	const float x = (backBufferWidth - viewportWidth) * 0.5f;
	const float y = (backBufferHeight - viewportHeight) * 0.5f;

	return D3D11_Viewport(x, y, viewportWidth, viewportHeight, 0.f, 1.f);
}
#include "pch.h"
#include "Timer.h"

Timer::Timer()
{
	// Windows XP 이후로는 실패하지 않고 반환값을 확인할 필요가 없다고 문서화되어 있다. 그래도 0 이면 나눗셈이 터지므로 방어한다.
	if (!::QueryPerformanceFrequency(&_frequency) || _frequency.QuadPart == 0)
		_frequency.QuadPart = 1;
	::QueryPerformanceCounter(&_last);
}

void Timer::Tick()
{
	LARGE_INTEGER now;
	::QueryPerformanceCounter(&now);

	// 카운트 차이를 먼저 정수로 구하고 나서 초로 바꾼다 — 큰 절대값(부팅 후 카운트)을 float 로 바꾸면 정밀도가 사라진다.
	const LONGLONG elapsed = now.QuadPart - _last.QuadPart;
	_last = now;

	float delta = static_cast<float>(static_cast<double>(elapsed) / static_cast<double>(_frequency.QuadPart));
	if (delta < 0.f) delta = 0.f;                             // QPC 는 단조 증가지만, 방어
	if (delta > kMaxDeltaTime) delta = kMaxDeltaTime;         // 창 드래그 후 첫 프레임 (Timer.h 의 "델타 clamp")

	_deltaTime = delta;
	_totalTime += static_cast<double>(delta);
}

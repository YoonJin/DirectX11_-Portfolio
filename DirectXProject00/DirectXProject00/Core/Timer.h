#pragma once

// QueryPerformanceCounter 기반 고해상도 타이머. (8단계)
// Execute 가 매 프레임 Tick() 하고, 그 프레임 동안 모두가 GetDeltaTime() 을 읽는다.
//
// [왜 GetTickCount / std::chrono::system_clock 이 아닌가]
//   GetTickCount 는 ~15ms 해상도라 60Hz 프레임(16.7ms)조차 제대로 못 잰다. system_clock 은 벽시계라 NTP 보정으로 뒤로 갈 수 있다.
//   steady_clock 도 되지만 Windows 에서는 결국 QPC 를 감싼 것이고, 이 프레임워크는 Win32 층을 직접 쓰는 것이 방침이다.
//
// [왜 프레임 카운트가 아니라 초인가]
//   Present(1, 0) 의 VSync 에 기대 "1프레임 = 1틱" 으로 진행하면 모니터 주사율(60/144Hz)에 따라 애니메이션 속도가 달라진다.
//   델타 타임(초)으로 진행하면 VSync 를 꺼도, 주사율이 달라도 같은 속도다.
//
// [델타 clamp 가 필수인 이유]
//   창 테두리를 드래그하는 동안(WM_ENTERSIZEMOVE ~ WM_EXITSIZEMOVE) 메인 루프가 모달 루프에 잡혀 멈춘다. 놓는 순간의 첫 델타는
//   수 초가 되고, 애니메이션은 그 시간만큼 한 번에 수십 프레임을 건너뛴다(물리가 있다면 터널링). 최대치로 잘라 "긴 정지는 느린 한 프레임" 으로 만든다.
//
// [Settings 에 넣지 않는다] Settings 는 창 상태 전달 통로이지 서비스 컨테이너가 아니다. Execute 가 소유한다.
class Timer final
{
public:
	Timer();                       // 주파수 조회 + 시작 시각
	void  Tick();                  // 프레임 시작에 한 번. 델타 계산 (kMaxDeltaTime 으로 clamp)
	float GetDeltaTime() const { return _deltaTime; }   // 직전 Tick 과 이번 Tick 사이 (초, clamp 됨)
	float GetTotalTime() const { return static_cast<float>(_totalTime); }   // 생성 이후 누적 (초, clamp 된 델타의 합 — 벽시계와 다를 수 있다)

	static constexpr float kMaxDeltaTime = 0.1f;   // 10 FPS 에 해당. 이보다 긴 정지는 한 프레임으로 취급한다

private:
	LARGE_INTEGER _frequency{};   // 초당 카운트. 부팅 후 불변 (QueryPerformanceFrequency)
	LARGE_INTEGER _last{};
	float  _deltaTime = 0.f;
	double _totalTime = 0.0;   // float 누적이면 몇 시간 뒤 ULP 가 프레임 델타보다 커져 합이 멈춘다 (8192s 에서 ULP 0.5ms). double 은 수백 년까지 안전
};

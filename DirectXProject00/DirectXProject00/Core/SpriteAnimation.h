#pragma once
#include "TextureAtlas.h"   // AtlasFrame

// 프레임 목록을 일정 간격으로 넘기는 재생기. (8단계)
// 아틀라스를 소유하지 않고 AtlasFrame 포인터만 든다 — 아틀라스(Execute 소유)가 이 객체보다 오래 살아야 한다.
//
// [시간 기반] Update(deltaTime) 로 초 단위 누적. 프레임 레이트와 무관하게 같은 속도로 재생된다 (Timer.h 참고).
//   누적 시간이 secondsPerFrame 을 넘을 때마다 한 프레임 전진 — while 이라 델타가 크면 여러 프레임을 한 번에 건너뛴다.
//   (Timer 의 clamp 가 델타 상한을 잡아주므로 "창 드래그 후 수백 프레임 점프" 는 일어나지 않는다)
//
// [하지 않는 것] 프레임 이벤트(콜백), 블렌드 트리 — 게임 로직 단계. 회전/뒤집기 — 9단계.
class SpriteAnimation final
{
public:
	void Set(std::vector<const AtlasFrame*> frames, float secondsPerFrame, bool loop = true);
	void Update(float deltaTime);
	void Reset();

	const AtlasFrame* GetCurrentFrame() const;   // 프레임이 없으면 nullptr
	bool IsFinished() const { return !_loop && _finished; }   // loop = false 일 때 마지막 프레임을 지났는가
	size_t GetFrameIndex() const { return _index; }

private:
	std::vector<const AtlasFrame*> _frames;
	float  _secondsPerFrame = 0.1f;
	float  _elapsed = 0.f;
	size_t _index = 0;
	bool   _loop = true;
	bool   _finished = false;
};

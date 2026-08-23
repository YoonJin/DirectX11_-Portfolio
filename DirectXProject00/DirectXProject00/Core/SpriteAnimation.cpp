#include "pch.h"
#include "SpriteAnimation.h"

void SpriteAnimation::Set(std::vector<const AtlasFrame*> frames, float secondsPerFrame, bool loop)
{
	_frames = std::move(frames);
	assert(secondsPerFrame > 0.f && "SpriteAnimation::Set: secondsPerFrame must be positive");
	_secondsPerFrame = secondsPerFrame > 0.f ? secondsPerFrame : 0.1f;   // Release 방어 : 0 이면 Update 의 while 이 끝나지 않는다
	_loop = loop;
	Reset();
}

void SpriteAnimation::Reset()
{
	_elapsed = 0.f;
	_index = 0;
	_finished = false;
}

void SpriteAnimation::Update(float deltaTime)
{
	if (_frames.empty() || _finished) return;

	_elapsed += deltaTime;
	// 한 프레임 시간을 넘길 때마다 전진. 나머지 시간은 보존한다 (0 으로 리셋하면 프레임마다 오차가 누적돼 느려진다).
	while (_elapsed >= _secondsPerFrame)
	{
		_elapsed -= _secondsPerFrame;
		++_index;
		if (_index >= _frames.size())
		{
			if (_loop)
			{
				_index = 0;
			}
			else
			{
				_index = _frames.size() - 1;   // 마지막 프레임에 머문다
				_finished = true;
				_elapsed = 0.f;
				break;
			}
		}
	}
}

const AtlasFrame* SpriteAnimation::GetCurrentFrame() const
{
	return _frames.empty() ? nullptr : _frames[_index];
}

#pragma once
#include <memory>
#include "SpriteBatch.h"   // SortMode (9단계 데모 상태)
#include "DemoScene.h"     // DemoScene (10-A 데모 셸)

class TextureManager;
class ShaderManager;
class RenderStates;
class Texture;
class TestPattern;
class Timer;
class TextureAtlas;
class SpriteAnimation;

class Execute final
{
public:
	Execute();
	~Execute();

	void Update();
	void Render();

private:
	// [소유 순서 = 생성 순서, 삭제는 역순] graphics → textureManager → shaderManager → renderStates → testPattern → spriteBatch
	// 관리자는 Graphics 다음으로 오래 살고, 사용자(testPattern/spriteBatch)는 관리자에서 raw 포인터로 빌려 쓰므로 관리자보다 먼저 죽어야 한다.
	Graphics* graphics = nullptr;
	TextureManager* textureManager = nullptr;   // 경로 → Texture 캐시 (6단계). Graphics 에 넣지 않는다 — Graphics 는 얇게
	ShaderManager* shaderManager = nullptr;     // 경로+엔트리 → Shader 캐시 (6단계)
	RenderStates* renderStates = nullptr;       // 블렌드/샘플러/래스터 프리셋 (7단계)
	TestPattern* testPattern = nullptr;         // 리사이즈 검증용 절차적 패턴
	SpriteBatch* spriteBatch = nullptr;         // 2D 스프라이트 렌더러 (5단계)

	// 텍스처는 textureManager 소유. Execute 는 포인터만 든다 (관리자보다 먼저 죽으므로 항상 유효).
	// [0] = image/0.png (5단계 물약), [1..3] = image/1~3.png — "관리자가 여러 장을 다룬다" 는 최소 확인용 (6단계).
	static constexpr int kIconCount = 4;
	Texture* icons[kIconCount] = {};

	// 8단계. 마지막에 만들고 먼저 지운다 — ~Execute 가 첫 줄에서 명시적으로 reset() 한다 (unique_ptr 멤버의 자동 소멸은 소멸자 본문 *뒤* 라서,
	// 그냥 두면 관리자/디바이스보다 늦게 죽는다. 지금은 소멸자가 아무것도 역참조하지 않아 무해하지만 "사용자는 관리자보다 먼저" 규칙을 코드로 지킨다).
	// animation 은 atlas 의 AtlasFrame* 를, atlas 는 textureManager 의 Texture* 를 빌려 든다 → animation → atlas → timer 순으로 놓는다.
	std::unique_ptr<Timer> timer;               // QPC 델타 타임. Update() 첫 줄에서 Tick()
	std::unique_ptr<TextureAtlas> atlas;        // image/items_atlas.txt (시트 텍스처는 textureManager 소유)
	std::unique_ptr<SpriteAnimation> animation; // item_00..11 순환

	// ---- 9단계 스트레스 데모 ----
	// 아틀라스 프레임 N 개를 고정 시드 랜덤 위치에 그린다. 위치는 한 번 정하고 유지한다 (매 프레임 랜덤이면 눈이 아프고 전/후 비교가 안 된다).
	// +/- 로 단계(0 → 100 → 1,000 → 5,000 → 10,000), S 로 정렬 모드 순환. 통계는 창 제목에 0.5초마다.
	// 배치는 "현재 논리 해상도" 안에 고정 시드로 깔리므로, 논리 해상도가 바뀌면(F1~F3) 다시 깐다 — 그래야 같은 단계 = 같은 그림이 유지된다.
	struct StressSprite { XMFLOAT2 position; uint frame; float rotation; };
	std::vector<StressSprite> stress;
	int stressLevel = 0;                          // kStressLevels 의 인덱스
	SortMode stressSort = SortMode::Deferred;
	void RebuildStress();                         // stressLevel 에 맞춰 stress 를 다시 만든다 (같은 시드 → 앞부분은 같은 배치)

	// ---- 10-A 데모 셸 ----
	// 장면은 메뉴 / 숫자키 → Settings::RequestScene → Update() 에서 소비. Render() 는 공통 배경(testPattern) 뒤에 DrawScene() 만 부른다.
	// 장면 0 = 9단계까지의 Render() 본문을 그대로 옮긴 DrawSceneAll() — 회귀 확인용이자 "전부 한 번에" 장면. 이 함수는 건드리지 않는다.
	// 장면 1·2 는 토글(10-C) 이라 아직 장면 0 과 같고, 4~7 은 10-B 에서 채웠다(아래). 3·8 은 좌/우 자리표시자(10-C 에서 내용이 들어간다).
	DemoScene scene = DemoScene::All;
	void DrawScene();                             // switch (scene) → DrawSceneAll / 10-B 장면 함수 / 자리표시자
	void DrawSceneAll();                          // 9단계까지의 Render() 본문 그대로
	void DrawScenePlaceholder();                  // 좌/우 각각 icons[0] 한 장 + 구분선. 메뉴·키·제목·분할 오프셋 확인용
	void DrawSplitGuide();                        // 가운데 세로선 (논리 x = HalfWidth() ± 1, 반투명 흰색)

	// ---- 10-B 장면 4~7 : "전" 을 새로 구현할 필요가 없는 장면들 ----
	// 좌/우가 전부 이미 있는 프리셋·정렬 모드·텍스처의 *인자 차이* 뿐이다. 규칙 :
	//   - 장면 함수 하나가 for (side 0..1) 로 양쪽을 그린다. 같은 코드가 side 에 따라 인자만 바꾸므로 "다른 건 다 같고 하나만 다르다" 가 실수 없이 보장된다.
	//   - 반쪽 안의 배치는 반쪽 좌상단 기준 상대 좌표로 적고 HalfOriginX(side) 를 더한다. F2(반쪽 400x600) 에서도 안 잘리도록 x 는 400 안에 둔다.
	//   - 반쪽마다 Begin/End 를 따로 — 7단계 규약 "한 Begin/End = 한 상태" (좌/우의 블렌드·샘플러·정렬이 다르다). 끝에 DrawSplitGuide().
	void DrawSceneBlendModes();                   // 4 : 좌 Opaque / 우 AlphaBlend + Additive 겹침
	void DrawSceneSamplers();                     // 5 : 좌 PointClamp / 우 LinearClamp — 축소 2장 + src 로 잘라 4배 확대
	void DrawSceneAtlas();                        // 6 : 좌 개별 텍스처 4장(draws 4) / 우 시트 한 장(draws 1)
	void DrawSceneYSort();                        // 7 : 좌 Deferred / 우 BackToFront — 같은 겹침 쌍의 위아래가 뒤집힌다

	// [좌/우 드로우콜은 GetStats()(직전 End 의 통계)로 센다] GetFrameStats() 는 프레임 합이라 반쪽을 따로 못 센다.
	// 장면 함수가 왼쪽 End() 직후 GetStats().drawCalls 를 leftDraws 에, 오른쪽 것을 rightDraws 에 보관하고 UpdateTitle() 이 찍는다.
	// 장면 6(과 10-C 의 8)만 의미가 있으므로 그 장면에서만 제목에 나온다. SpriteBatch 는 안 바꾼다.
	uint leftDraws = 0;
	uint rightDraws = 0;

	// [좌/우 분할은 뷰포트 2개가 아니라 논리 좌표 오프셋이다]
	//   뷰포트를 둘로 나누면 레터박스 계산·투영 행렬·TestPattern 이 전부 두 벌이 된다. 대신 장면 함수가 왼쪽(side 0, 적용 전)은 x 오프셋 0,
	//   오른쪽(side 1, 적용 후)은 논리 폭의 절반을 더해 그린다. SpriteBatch 와 투영은 한 줄도 안 바뀐다.
	//   반쪽 하나의 논리 원점은 (side * HalfWidth(), 0), 폭 HalfWidth(), 높이 = 논리 높이. 1280x720 이면 640x720, F2(800x600) 면 400x600.
	float HalfOriginX(int side) const { assert(side == 0 || side == 1); return side * Settings::Get().GetLogicalWidth() * 0.5f; }
	float HalfWidth() const { return Settings::Get().GetLogicalWidth() * 0.5f; }

	// ---- 통계 표시 ----
	// CPU 프레임 시간은 Timer 의 델타(Present 의 VSync 대기 포함)와 별도로 Update+Render 구간을 QPC 로 직접 잰다 — VSync 대기가 섞이면 의미가 없다.
	LARGE_INTEGER qpcFrequency = {};
	LARGE_INTEGER frameStart = {};
	double cpuFrameMsAccum = 0.0;                 // 0.5초 창의 평균을 낸다
	uint   cpuFrameCount = 0;
	float  titleTimer = 0.f;
	bool   titleDirty = true;                     // 첫 프레임과 키 입력 직후에는 0.5초를 기다리지 않고 바로 갱신
	void UpdateTitle();
};

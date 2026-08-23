#include "pch.h"
#include "Execute.h"
#include "TextureManager.h"
#include "ShaderManager.h"
#include "RenderStates.h"
#include "TestPattern.h"
#include "SpriteBatch.h"
#include "Timer.h"
#include "TextureAtlas.h"
#include "SpriteAnimation.h"
#include <random>

Execute::Execute()
{
	// 리소스들을 생성 초기화 해준다.
	graphics = new Graphics();
	graphics->CreateDeviceAndSwapChain();
	graphics->CreateRenderTargetView(
		static_cast<uint>(Settings::Get().GetWidth()),
		static_cast<uint>(Settings::Get().GetHeight()));
	graphics->SetViewport(
		static_cast<uint>(Settings::Get().GetWidth()),
		static_cast<uint>(Settings::Get().GetHeight()));

	// 디바이스가 준비된 뒤에 만든다 (셰이더/텍스처/버퍼를 디바이스로 생성하므로). 관리자가 먼저, 사용자가 나중.
	textureManager = new TextureManager(graphics);
	shaderManager = new ShaderManager(graphics);
	renderStates = new RenderStates(graphics);
	testPattern = new TestPattern(graphics, shaderManager, renderStates);
	spriteBatch = new SpriteBatch(graphics, textureManager, shaderManager, renderStates);

	// 테스트 에셋. 255x255 알파 아이콘. 경로는 리포 루트 상대 — 관리자가 Path::AssetRoot() 기준으로 정규화한다.
	// Load 실패는 nullptr 로 돌아오고(관리자가 절대 경로를 출력창에 찍는다), SpriteBatch::Draw 는 null 을 건너뛴다.
	// 포트폴리오 데모의 필수 에셋이므로 여기서는 assert 로 바로 드러낸다.
	for (int i = 0; i < kIconCount; ++i)
	{
		icons[i] = textureManager->Load(L"image/" + std::to_wstring(i) + L".png");
		assert(icons[i] && "Execute: icon texture load failed (see debug output for the absolute path)");
	}
	// (6단계 검증 때 여기서 같은 파일 두 번 Load → 같은 포인터 / 대소문자·구분자 무시 / 없는 경로 → nullptr 를 assert 로 확인하고 제거했다)

	// 8단계 : 타이머 + 아틀라스 + 애니메이션. 아틀라스 정의가 시트(image/items_atlas.png)를 textureManager 로 로드한다.
	timer = std::make_unique<Timer>();
	atlas = std::make_unique<TextureAtlas>();
	if (!atlas->LoadFromFile(textureManager, L"image/items_atlas.txt"))
		assert(false && "Execute: image/items_atlas.txt failed to load (run image/pack_items.ps1; see debug output)");
	animation = std::make_unique<SpriteAnimation>();
	// 12장이 0.15s 간격으로 순환 — 한 바퀴 1.8s. 아이템 슬롯이 룰렛처럼 도는 느낌.
	animation->Set(atlas->GetFrames("item_"), 0.15f, true);
	assert(animation->GetCurrentFrame() && "Execute: atlas has no item_* frames");

	// 9단계 : 통계용 QPC + 스트레스 데모 초기 상태(0개). 창 제목은 첫 프레임에 바로 갱신된다.
	::QueryPerformanceFrequency(&qpcFrequency);
	RebuildStress();
}

namespace
{
	constexpr uint kStressLevels[] = { 0, 100, 1000, 5000, 10000 };
	constexpr int  kStressLevelCount = static_cast<int>(sizeof(kStressLevels) / sizeof(kStressLevels[0]));
	constexpr int  kSortModeCount = 4;   // SortMode 열거자 수 (Deferred, Texture, BackToFront, FrontToBack). SortModeName 과 같이 유지

	const wchar_t* SortModeName(SortMode mode)
	{
		switch (mode)
		{
		case SortMode::Deferred:    return L"Deferred";
		case SortMode::Texture:     return L"Texture";
		case SortMode::BackToFront: return L"BackToFront";
		case SortMode::FrontToBack: return L"FrontToBack";
		}
		return L"?";
	}
}

void Execute::RebuildStress()
{
	// 고정 시드 : 단계를 올렸다 내려도 같은 배치가 나온다 (앞 N 개는 항상 같다). 전/후 비교·스크린샷 비교의 전제.
	const uint count = kStressLevels[stressLevel];
	const float w = static_cast<float>(Settings::Get().GetLogicalWidth());
	const float h = static_cast<float>(Settings::Get().GetLogicalHeight());
	std::mt19937 rng(20260823u);
	std::uniform_real_distribution<float> px(0.f, w), py(64.f, h);   // 발 y 가 64 이상 — 위쪽 잘림을 줄인다
	std::uniform_int_distribution<uint> frame(0u, static_cast<uint>(atlas->GetFrameCount() > 0 ? atlas->GetFrameCount() - 1 : 0));
	std::uniform_real_distribution<float> rot(-0.3f, 0.3f);
	stress.resize(count);
	for (uint i = 0; i < count; ++i)
	{
		// 분포 객체를 같은 순서로 부르므로 앞 N 개는 단계와 무관하게 같은 값이다.
		const float x = px(rng), y = py(rng); const uint f = frame(rng); const float r = rot(rng);
		stress[i] = { { x, y }, f, r };
	}
}

void Execute::UpdateTitle()
{
	// 프레임 누적 통계 — CPU ms 와 같은 범위(프레임 전체, 데모 블록 포함). 스트레스 블록만 보려면 GetStats() (직전 End).
	const SpriteBatch::Stats& st = spriteBatch->GetFrameStats();
	const double ms = cpuFrameCount ? cpuFrameMsAccum / cpuFrameCount : 0.0;
	// [10-A 제목 형식] 텍스트 렌더링이 없으므로 창 제목이 자막이다. 장면 번호·이름·좌/우 설명 → 프레임 통계 → 키 안내.
	//   D2D11Game  |  [4/8] 렌더 상태 — 좌: Opaque / 우: AlphaBlend + Additive  |  frame: sprites 18  draws 12  cpu 0.13 ms avg  |  [0-8 장면, +/- S, F1-F3]
	// 장면 0 은 "[0] 전체 데모" 와 9단계의 스트레스 수치(단계·정렬 모드·flush 내역)를 그대로 보여준다.
	const int sceneIndex = static_cast<int>(scene);
	const int sceneLast = static_cast<int>(DemoScene::Count) - 1;
	wchar_t title[512];
	if (scene == DemoScene::All)
	{
		swprintf_s(title, L"D2D11Game  |  [0] 전체 데모  |  stress %u (%s)  frame: sprites %u  draws %u (batches %u, +tex %u, +cap %u)  cpu %.2f ms avg  |  [0-%d 장면, +/- S, F1-F3]",
		           kStressLevels[stressLevel], SortModeName(stressSort), st.sprites, st.drawCalls, st.batches, st.flushesByTexture, st.flushesByCapacity, ms, sceneLast);
	}
	else
	{
		// 메뉴 라벨 "&4 렌더 상태 — Opaque / AlphaBlend" 에서 앞의 "&4 " 와 뒤의 " — 전 / 후" 요약을 떼고 기능 이름만 쓴다
		// (좌/우 설명은 before/after 가 따로 들어가므로 겹치지 않게. 라벨이 단일 출처 — 같은 문자열을 두 번 적지 않는다).
		const wchar_t* label = kDemoScenes[sceneIndex].menuLabel;
		if (label[0] == L'&') label += 1;
		while (*label && *label != L' ') ++label;
		while (*label == L' ') ++label;
		const wchar_t* dash = wcsstr(label, L" — ");   // U+2014 EM DASH
		const int labelLength = dash ? static_cast<int>(dash - label) : static_cast<int>(wcslen(label));
		// 10-B : 장면 6(과 10-C 의 8)은 드로우콜 수가 곧 비교 포인트라 좌/우를 따로 보여준다 (장면 함수가 반쪽 End() 직후 GetStats() 로 센 값).
		wchar_t sideDraws[64] = L"";
		if (scene == DemoScene::Atlas)   // 10-C 에서 장면 8(Batching)이 값을 채우면 여기에 추가한다 — 채우지 않는 장면에 붙이면 묵은 값이 찍힌다
			swprintf_s(sideDraws, L"  좌 draws %u / 우 draws %u", leftDraws, rightDraws);
		swprintf_s(title, L"D2D11Game  |  [%d/%d] %.*s — 좌: %s / 우: %s  |  frame: sprites %u  draws %u%s  cpu %.2f ms avg  |  [0-%d 장면, +/- S, F1-F3]",
		           sceneIndex, sceneLast, labelLength, label, kDemoScenes[sceneIndex].before, kDemoScenes[sceneIndex].after, st.sprites, st.drawCalls, sideDraws, ms, sceneLast);
	}
	::SetWindowTextW(Settings::Get().GetWindowHandle(), title);
	cpuFrameMsAccum = 0.0;
	cpuFrameCount = 0;
}

Execute::~Execute()
{
	// 생성의 역순으로 지운다 : spriteBatch → testPattern → renderStates → shaderManager → textureManager → graphics.
	// 사용자(spriteBatch/testPattern)가 관리자보다 먼저 — 빌린 Shader*/Texture* 가 죽은 뒤에 쓰이는 일이 없다.
	// 관리자가 graphics 보다 먼저 — 셰이더/버퍼/텍스처가 디바이스보다 먼저 Release 되어야 ReportLiveDeviceObjects 에 Live ID3D11Device 한 줄만 남는다.
	// spriteBatch 는 소멸자에서 graphics 의 논리 해상도 리스너를 해제하므로, 이 순서가 곧 "죽은 객체의 콜백이 남지 않는다" 의 보장이다.
	// textureManager 는 WIC 팩토리도 들고 있다 — WinMain 의 CoUninitialize 보다 먼저 놓여야 하고, Execute 삭제가 그보다 앞이므로 맞는다.
	// 8단계 객체는 빌린 포인터(AtlasFrame* / Texture*)를 들고 있으므로 가장 먼저 — unique_ptr 의 자동 소멸은 이 본문 뒤라서 명시적으로 놓는다.
	animation.reset();
	atlas.reset();
	timer.reset();
	SAFE_DELETE(spriteBatch);
	SAFE_DELETE(testPattern);
	SAFE_DELETE(renderStates);   // 상태 객체 8개 — 전부 여기 멤버라 이 순서만 맞으면 Live 객체로 남지 않는다
	SAFE_DELETE(shaderManager);
	SAFE_DELETE(textureManager);
	SAFE_DELETE(graphics);
}

void Execute::Update()
{
	// 프레임 시작 : 델타 타임. 이 뒤의 모든 시간 기반 갱신(애니메이션)이 같은 델타를 본다.
	timer->Tick();
	::QueryPerformanceCounter(&frameStart);   // CPU 프레임 시간 측정 시작 (Render 끝에서 닫는다)

	// [창 크기 변경 처리]
	// WndProc 은 Settings 에 요청만 기록하고, 실제 GPU 리소스 재생성은 여기(메인 루프)에서 한다.
	// 이유는 Settings.h 의 "창 상태" 절 참고 (Graphics 접근 불가 / WM_SIZE 폭풍 병합 / Render() 와의 재진입 회피).
	// 드래그 중(IsSizing) 에는 처리하지 않고 WM_EXITSIZEMOVE 가 넣어주는 최종 요청을 기다린다.
	//
	// 주의 : ConsumeResizeRequest 는 반드시 IsSizing() 뒤에 와야 한다 (&& 단락 평가).
	//        앞에 두면 드래그 중에 요청이 소비되어 버려지고, 드래그가 끝나도 처리할 요청이 남지 않는다.
	//
	// 실행 직후 창 생성 시 오는 첫 WM_SIZE 는 생성자에서 이미 같은 크기로 만든 상태라
	// Resize() 의 "같은 크기면 무시" 조건에 걸려 아무 일도 하지 않는다. 정상이다.
	uint width = 0, height = 0;
	if (!Settings::Get().IsSizing() && Settings::Get().ConsumeResizeRequest(width, height))
	{
		if (!graphics->Resize(width, height))
		{
			// Release 빌드에서 ResizeBuffers / RTV 생성이 실패한 경우 (CHECK 가 사라져 여기까지 온다).
			// 요청은 이미 소비됐으므로 그대로 두면 다음 WM_SIZE 가 올 때까지 RTV 없는 상태로 남는다.
			// 요청을 되살려 다음 프레임에 다시 시도한다. (Render() 는 RTV 가 없으면 건너뛴다)
			Settings::Get().RequestResize(width, height);
		}

		// Settings 의 width/height 는 "현재 백버퍼 크기" 다. Resize() 가 실제로 만든 크기로 맞춘다.
		// (Resize() 가 0 크기/동일 크기를 걸러 아무것도 안 했거나 실패했더라도
		//  Graphics 가 들고 있는 값이 진실이므로 그쪽을 읽는다)
		Settings::Get().SetWidth(static_cast<float>(graphics->GetBackBufferWidth()));
		Settings::Get().SetHeight(static_cast<float>(graphics->GetBackBufferHeight()));
	}

	// [논리 해상도 변경 처리] (F1/F2/F3 데모)
	// 리사이즈와 같은 "요청 → 루프에서 처리" 패턴. Graphics::SetLogicalResolution() 이 뷰포트를 다시 계산하고
	// 논리 해상도 리스너(SpriteBatch 의 투영 행렬)에게 통지한다. 백버퍼는 건드리지 않는다.
	uint logicalWidth = 0, logicalHeight = 0;
	if (Settings::Get().ConsumeLogicalResolutionRequest(logicalWidth, logicalHeight))
	{
		graphics->SetLogicalResolution(logicalWidth, logicalHeight);
		RebuildStress();   // 배치는 논리 해상도 안에 깔린다 — 새 해상도로 다시 (Execute.h 의 스트레스 데모 주석)
		titleDirty = true;
	}

	// [9단계 데모 키] 스트레스 단계 / 정렬 모드. 논리 해상도 요청과 같은 통로.
	if (const int step = Settings::Get().ConsumeStressStep())
	{
		const int next = stressLevel + step;
		stressLevel = next < 0 ? 0 : (next >= kStressLevelCount ? kStressLevelCount - 1 : next);
		RebuildStress();
		titleDirty = true;
	}
	if (const int cycles = Settings::Get().ConsumeSortModeCycle())
	{
		// Deferred → Texture → BackToFront → FrontToBack → Deferred
		stressSort = static_cast<SortMode>((static_cast<int>(stressSort) + cycles) % kSortModeCount);
		titleDirty = true;
	}

	// [10-A 데모 장면] 메뉴 / 숫자키 요청. 장면이 바뀌면 메뉴의 라디오 체크를 따라 옮기고 제목을 바로 갱신한다.
	// 스트레스 데모 상태(stressLevel/stressSort)는 건드리지 않는다 — 장면 0 에서만 의미 있고, 돌아왔을 때 그대로여야 전/후 비교가 이어진다.
	// Execute → 창(HWND) 방향의 호출(CheckMenuRadioItem)은 허용된다. 금지는 반대 방향(WndProc → Graphics/Execute) 뿐이다.
	int requestedScene = 0;
	if (Settings::Get().ConsumeSceneRequest(requestedScene))
	{
		if (requestedScene >= 0 && requestedScene < static_cast<int>(DemoScene::Count) && static_cast<DemoScene>(requestedScene) != scene)
		{
			scene = static_cast<DemoScene>(requestedScene);
			if (const HMENU menu = ::GetMenu(Settings::Get().GetWindowHandle()))
			{
				// MF_BYCOMMAND : ID 로 찾는다. 메뉴 바 핸들을 넘겨도 하위 팝업까지 뒤져 주므로 팝업 핸들을 보관할 필요가 없다.
				::CheckMenuRadioItem(menu, kSceneMenuIdBase, kSceneMenuIdBase + static_cast<UINT>(DemoScene::Count) - 1,
				                     kSceneMenuIdBase + static_cast<UINT>(scene), MF_BYCOMMAND);
			}
			titleDirty = true;
		}
	}

	// [게임 업데이트] 창 요청 처리가 끝난 뒤. 지금은 애니메이션 하나.
	animation->Update(timer->GetDeltaTime());
	// (8단계 검증 때 여기서 GetFrame("없는이름") == nullptr / GetFrames 접두사 정렬 / loop=false 의 IsFinished 를 assert 로 확인하고 제거했다.
	//  프레임 간격 0.150s·한 바퀴 1.8s 와 드래그 후 델타 clamp(0.1s) 는 OutputDebugString 타임스탬프로 확인했다)
}

void Execute::Render()
{
	// 최소화 상태에서는 클라이언트 영역이 0x0 이라 백버퍼를 맞출 수도, 그릴 곳도 없다. 건너뛴다.
	// 이때 Present(1, 0) 의 VSync 대기도 함께 사라져 PeekMessage 루프가 코어 하나를 100% 돌리게 되므로,
	// 잠깐 양보해서 CPU 를 태우지 않는다.
	if (Settings::Get().IsMinimized())
	{
		Sleep(16);
		return;
	}

	// Release 빌드에서 리사이즈가 실패해 RTV 가 없는 프레임은 그리지 않는다. (Update() 가 재시도 요청을 넣어둔다)
	if (!graphics->HasRenderTarget()) return;

	spriteBatch->ResetFrameStats();   // 이 프레임의 모든 Begin/End 를 합산한다 (제목 표시용)
	graphics->RenderBegin();
	// 렌더링 파이프라인에 따른 여러가지 처리들을 아래에 기재
	{
		// 체커보드는 모든 장면의 공통 배경 — 먼저 그리고 그 위에 스프라이트를 올린다. 레터박스 경계와 스프라이트 위치의 관계가 눈에 보여야 한다.
		testPattern->Draw();
		// 10-A : 현재 장면. 장면 0 = 9단계까지의 데모 그대로(DrawSceneAll), 나머지는 좌/우 분할.
		DrawScene();
	}
	// [CPU 프레임 시간] Update 시작 ~ Present 직전. Present(1,0) 의 VSync 대기는 여기 들어가지 않는다 — 그 대기가 섞이면 CPU 비용 비교가 의미를 잃는다.
	// (Blt 모델 창 모드에서는 Present 가 거의 블록하지 않는다 — 8단계에서 ~5ms/프레임으로 도는 것을 확인했다 — 그래도 규칙은 지킨다)
	LARGE_INTEGER now;
	::QueryPerformanceCounter(&now);
	cpuFrameMsAccum += 1000.0 * static_cast<double>(now.QuadPart - frameStart.QuadPart) / static_cast<double>(qpcFrequency.QuadPart);
	++cpuFrameCount;

	graphics->RenderEnd();

	// 0.5초마다 (또는 키 입력 직후 한 번) 제목 갱신. SetWindowText 는 동기 WM_SETTEXT + 비클라이언트 다시 그리기라 매 프레임 부르면 안 된다.
	titleTimer += timer->GetDeltaTime();
	if (titleTimer >= 0.5f || titleDirty)
	{
		titleTimer = 0.f;
		titleDirty = false;
		UpdateTitle();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// 10-A 데모 셸 : 장면 분기 + 좌/우 분할 헬퍼
// ---------------------------------------------------------------------------------------------------------------------

void Execute::DrawScene()
{
	switch (scene)
	{
	case DemoScene::All:
		DrawSceneAll();
		break;
	case DemoScene::Resize:
	case DemoScene::Letterbox:
		// 백버퍼/뷰포트 자체가 주제라 분할이 아니라 스페이스 토글로 간다 (10-C). 스위치가 생기기 전까지는 장면 0 과 같은 그림.
		DrawSceneAll();
		break;
	case DemoScene::BlendModes: DrawSceneBlendModes(); break;   // 10-B
	case DemoScene::Samplers:   DrawSceneSamplers();   break;   // 10-B
	case DemoScene::Atlas:      DrawSceneAtlas();      break;   // 10-B
	case DemoScene::YSort:      DrawSceneYSort();      break;   // 10-B
	default:
		// 3·8 : 10-C 에서 "전" 재현 스위치(straight 텍스처, SetMaxBatchSize)와 함께 들어간다. 지금은 자리표시자.
		DrawScenePlaceholder();
		break;
	}
}

void Execute::DrawScenePlaceholder()
{
	// 왼쪽 반과 오른쪽 반에 같은 물약을 같은 상대 위치에 — 구분선이 정확히 가운데이고 양쪽이 대칭이면 오프셋이 맞는 것이다.
	// 반쪽 폭의 중앙에 두되, 논리 해상도가 작아도(F2 = 400x600 반쪽) 잘리지 않도록 크기는 반쪽 폭의 절반으로 한다 (1280x720 이면 320x320).
	const float size = HalfWidth() * 0.5f;
	const float y = (static_cast<float>(Settings::Get().GetLogicalHeight()) - size) * 0.5f;
	spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend);
	for (int side = 0; side < 2; ++side)
		spriteBatch->Draw(icons[0], { HalfOriginX(side) + (HalfWidth() - size) * 0.5f, y, size, size }, nullptr, 0xFFFFFFFF);
	spriteBatch->End();
	DrawSplitGuide();
}

void Execute::DrawSplitGuide()
{
	// 가운데 세로선. 논리 x = HalfWidth() 를 중심으로 2 논리 단위 폭 (1280 이면 639..641), 반투명 흰색이라 체커보드 위에서도 보인다.
	// 장면 본문이 그려진 *뒤* 에 부르면 선이 항상 위에 온다 — 분할 경계가 스프라이트에 가려지지 않는다.
	const float h = static_cast<float>(Settings::Get().GetLogicalHeight());
	spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend);
	spriteBatch->DrawRect({ HalfWidth() - 1.f, 0.f, 2.f, h }, 0xA0FFFFFF);
	spriteBatch->End();
}

// ---------------------------------------------------------------------------------------------------------------------
// 10-B 장면 4~7 : 전/후가 기존 프리셋·정렬·텍스처의 인자 차이뿐인 장면들.
// 공통 : 한 함수가 for (side) 로 양쪽을 그리고, 반쪽 상대 좌표 + HalfOriginX(side). 반쪽마다 Begin/End (한 Begin/End = 한 상태).
// 배치는 반쪽 폭이 가장 좁은 F2(800x600 → 반쪽 400x600) 에서도 잘리지 않도록 x < 400, y < 600 안에 둔다.
// ---------------------------------------------------------------------------------------------------------------------

void Execute::DrawSceneBlendModes()
{
	// 좌 : Opaque — 블렌드 꺼짐. 프리멀티플라이드 텍스처의 투명 영역은 RGB 가 0 이라 *검정 사각형* 으로 보인다 (straight 알파였다면 쓰레기 색).
	//      반투명 파랑 사각형도 알파가 무시되어 불투명 파랑이 된다.
	// 우 : AlphaBlend — 체커보드가 비치고 사각형이 반투명. 그 아래 Additive 로 같은 물약 두 장을 겹치면 겹친 부분이 밝아진다(ONE/ONE).
	for (int side = 0; side < 2; ++side)
	{
		const float ox = HalfOriginX(side);
		const BlendMode mode = side == 0 ? BlendMode::Opaque : BlendMode::AlphaBlend;
		spriteBatch->Begin(SortMode::Deferred, mode, SamplerMode::PointClamp);
		spriteBatch->Draw(icons[0], { ox + 72.f, 40.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);   // 물약 원본 크기 (255x255)
		spriteBatch->DrawRect({ ox + 100.f, 200.f, 200.f, 100.f }, 0x80FF0000);                 // 반투명 파랑(0xAABBGGRR) — 물약 아래쪽에 걸쳐서.
		                                                                                        // Opaque 쪽이 "어두운" 파랑인 이유 : Draw() 가 색을 프리멀티플라이하므로 RGB 가 (0,0,128) 로 들어가고 알파는 무시된다
		spriteBatch->End();
		if (side == 1)
		{
			// 후 쪽에만 : Additive 두 장 겹침. 보통(알파 1) 텍스처를 더하기로 그리면 겹친 부분이 흰색 쪽으로 포화한다.
			spriteBatch->Begin(SortMode::Deferred, BlendMode::Additive, SamplerMode::PointClamp);
			spriteBatch->Draw(icons[0], { ox + 40.f,  320.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);
			spriteBatch->Draw(icons[0], { ox + 140.f, 340.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);
			spriteBatch->End();
		}
	}
	DrawSplitGuide();
}

void Execute::DrawSceneSamplers()
{
	// 같은 아이콘을 1/4·1/2 축소 + 4배 확대. 좌 PointClamp 는 축소본이 거칠고 확대본이 계단식, 우 LinearClamp 는 둘 다 부드럽다(뭉개진다).
	// 4배 확대는 src 인자(텍셀 사각형)로 물약 위쪽 "해골 마개" 64x64 텍셀만 잘라 256x256 으로 — src 의 두 번째 사용자 (첫 번째는 8단계 아틀라스).
	// 확대 영역이 양쪽 차이를 가장 크게 보여 준다 : 1 텍셀 = 4 논리 단위라 포인트 샘플링의 블록이 그대로 보인다.
	const RECT_F skull = { 112.f, 0.f, 64.f, 64.f };   // image/0.png 에서 해골이 있는 텍셀 사각형 (x 112..176, y 0..64)
	for (int side = 0; side < 2; ++side)
	{
		const float ox = HalfOriginX(side);
		const SamplerMode sampler = side == 0 ? SamplerMode::PointClamp : SamplerMode::LinearClamp;
		spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend, sampler);
		spriteBatch->Draw(icons[0], { ox + 40.f,  80.f,  64.f,  64.f }, nullptr, 0xFFFFFFFF);   // 1/4 축소
		spriteBatch->Draw(icons[0], { ox + 140.f, 80.f, 128.f, 128.f }, nullptr, 0xFFFFFFFF);   // 1/2 축소
		spriteBatch->Draw(icons[0], { ox + 40.f, 240.f, 256.f, 256.f }, &skull, 0xFFFFFFFF);    // 4배 확대
		spriteBatch->End();
	}
	DrawSplitGuide();
}

void Execute::DrawSceneAtlas()
{
	// 좌 : 개별 텍스처 4장. 9단계 배칭이 있어도 텍스처가 바뀔 때마다 flush 라 드로우콜 4.
	// 우 : 시트 한 장 — 썸네일 + 순환 프레임 + 좌우 반전. 전부 같은 텍스처라 드로우콜 1. 제목의 "좌 draws 4 / 우 draws 1" 이 비교 포인트.
	// 두 프레임의 피벗(발)을 같은 y 에 두므로 아래 가장자리가 같은 y — 피벗이 반전에도 유지되는지 확인.
	for (int side = 0; side < 2; ++side)
	{
		const float ox = HalfOriginX(side);
		spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend, SamplerMode::PointClamp);
		if (side == 0)
		{
			// 4장을 2x2 로 (F2 의 반쪽 폭 400 안에 들어가게). 128x128, 간격 150.
			for (int i = 0; i < kIconCount; ++i)
				spriteBatch->Draw(icons[i], { ox + 40.f + 150.f * (i % 2), 80.f + 150.f * (i / 2), 128.f, 128.f }, nullptr, 0xFFFFFFFF);
			spriteBatch->End();
			leftDraws = spriteBatch->GetStats().drawCalls;    // 4
		}
		else
		{
			if (const Texture* sheet = atlas->GetTexture())
			{
				spriteBatch->Draw(sheet, { ox + 40.f, 80.f, 340.f, 255.f }, nullptr, 0xFFFFFFFF);   // 시트 썸네일 (1020x765 → 1/3). 셀 순서 = 재생 순서
				if (const AtlasFrame* frame = animation->GetCurrentFrame())
				{
					spriteBatch->Draw(sheet, *frame, { ox + 120.f, 580.f }, 0.7f);                                                   // 발 y = 580 에 순환 프레임 (0.7배 = 178 폭, 둘이 안 겹치게)
					spriteBatch->Draw(sheet, *frame, { ox + 300.f, 580.f }, 0.7f, 0xFFFFFFFF, 0.f, SpriteEffects::FlipHorizontally);   // 같은 발 y, 좌우 반전
				}
			}
			spriteBatch->End();
			rightDraws = spriteBatch->GetStats().drawCalls;   // 1
		}
	}
	DrawSplitGuide();
}

void Execute::DrawSceneYSort()
{
	// 9단계의 겹침 쌍을 양쪽에 같은 배치로, 정렬 모드만 다르게. 일부러 "앞(발 y 큼)" 을 먼저 그린다 :
	//   좌 Deferred    = 호출 순서. 나중에 그린 뒤쪽(발 y 작은) 아이콘이 위를 덮는다 — 틀린 겹침.
	//   우 BackToFront = depth(= 발 y) 오름차순으로 다시 정렬. 발이 아래인 것이 위에 온다 — y-sort.
	// 정지 장면이므로 stable_sort 검증도 여기서 눈으로 된다 : 같은 발 y 의 두 장(frames[2], frames[8])이 깜빡이지 않고 호출 순서를 유지해야 한다.
	const Texture* sheet = atlas->GetTexture();
	const std::vector<const AtlasFrame*> frames = atlas->GetFrames("item_");
	for (int side = 0; side < 2; ++side)
	{
		const float ox = HalfOriginX(side);
		const SortMode sort = side == 0 ? SortMode::Deferred : SortMode::BackToFront;
		spriteBatch->Begin(sort, BlendMode::AlphaBlend, SamplerMode::PointClamp);
		if (sheet && frames.size() >= 12)
		{
			// 피벗이 x 127 이므로 프레임의 좌우 폭은 position.x ± 127 — 반쪽(400) 안에 들려면 x 는 128..272 사이여야 한다.
			spriteBatch->Draw(sheet, *frames[0],  { ox + 200.f, 420.f }, 1.f);   // 물약,     발 y 420 (앞) — 먼저 그림
			spriteBatch->Draw(sheet, *frames[11], { ox + 270.f, 330.f }, 1.f);   // 회중시계, 발 y 330 (뒤) — 나중에 그림 → Deferred 에서는 위에 온다
			spriteBatch->Draw(sheet, *frames[5],  { ox + 140.f, 360.f }, 1.f);   // 하나 더 (세 장이면 순서 역전이 더 잘 보인다)
			spriteBatch->Draw(sheet, *frames[2],  { ox + 120.f, 560.f }, 0.5f);  // 같은 발 y 560 두 장 — stable_sort : 나중에 그린 frames[8] 이 항상 위
			spriteBatch->Draw(sheet, *frames[8],  { ox + 180.f, 560.f }, 0.5f);
		}
		spriteBatch->End();
	}
	DrawSplitGuide();
}

void Execute::DrawSceneAll()
{
	// 9단계까지의 Render() 본문을 그대로 옮긴 것 (10-A). 회귀 확인용 — 장면 0 은 9단계 화면과 픽셀 단위로 같아야 한다.
	// 7~9단계 데모를 바꾸려면 각 장면(10-B/10-C)으로 가고, 여기는 건드리지 않는다.
	// 7단계 데모 장면 : 같은 물약(image/0.png)을 블렌드 프리셋별로 나란히. 한 Begin/End 는 한 상태라 블록을 나눈다.
	const Texture* potion = icons[0];

	// AlphaBlend — 5단계와 동일. 투명 영역으로 체커보드가 비친다. 반투명 파란 사각형(straight 알파 0xAABBGGRR → 내부에서 프리멀티플라이)도 여기.
	// 6단계의 image/1~3.png 줄(축소해서 논리 y = 550)도 이 블록 안이다.
	spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend);
	spriteBatch->Draw(potion, { 100.f, 100.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->DrawRect({ 400.f, 400.f, 200.f, 100.f }, 0x80FF0000);
	for (int i = 1; i < kIconCount; ++i)
		spriteBatch->Draw(icons[i], { 100.f + 200.f * i, 550.f, 128.f, 128.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->End();

	// Additive — 두 장이 겹치는 부분이 밝아진다 (보라색 병이 겹치면 분홍/흰색에 가까워진다). 일반 텍스처(알파 1)를 가산으로 그리는 경우.
	spriteBatch->Begin(SortMode::Deferred, BlendMode::Additive);
	spriteBatch->Draw(potion, { 400.f, 100.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->Draw(potion, { 460.f, 160.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->End();

	// Opaque — 투명 영역이 검정 사각형. 프리멀티플라이드 텍스처의 투명 픽셀은 RGB 가 (0,0,0) 이기 때문이다 (straight 였다면 쓰레기 색).
	spriteBatch->Begin(SortMode::Deferred, BlendMode::Opaque);
	spriteBatch->Draw(potion, { 800.f, 100.f, 255.f, 255.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->End();

	// 샘플러 비교 — 64x64 로 축소. 선형은 부드럽고 포인트는 각진다 (1px 체커보드 위라 차이가 또렷하다).
	spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend, SamplerMode::LinearClamp);
	spriteBatch->Draw(potion, { 100.f, 400.f, 64.f, 64.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->End();
	spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend, SamplerMode::PointClamp);
	spriteBatch->Draw(potion, { 200.f, 400.f, 64.f, 64.f }, nullptr, 0xFFFFFFFF);
	spriteBatch->End();

	// 8단계 : 아틀라스 + 애니메이션. 시트 한 장(items_atlas.png)에서 프레임을 잘라 쓴다 — SpriteBatch 는 src 로 이미 할 수 있었다.
	if (const Texture* sheet = atlas->GetTexture())
	{
		spriteBatch->Begin(SortMode::Deferred, BlendMode::AlphaBlend, SamplerMode::PointClamp);
		// 논리 (640, 650) 에 "발"(아이콘 아래 중앙 = 피벗 127,255)을 두고 1배. 아이콘의 아래 가장자리가 항상 y = 650 에 고정되어야 한다.
		if (const AtlasFrame* frame = animation->GetCurrentFrame())
			spriteBatch->Draw(sheet, *frame, { 640.f, 650.f }, 1.f);
		// 시트 전체를 축소해서 (1020×765 → 340×255) — 프레임 잘라내기가 맞는지 비교용. 재생 순서 = 셀 순서(좌→우, 위→아래).
		spriteBatch->Draw(sheet, { 880.f, 400.f, 340.f, 255.f }, nullptr, 0xFFFFFFFF);
		// 정지 프레임 하나를 이름으로 직접 꺼내기 — GetFrame 검증. item_07 = 시트의 2행 4열.
		if (const AtlasFrame* still = atlas->GetFrame("item_07"))
			spriteBatch->Draw(sheet, *still, { 1000.f, 700.f }, 0.5f);
		// 9단계 : 변환. 순환 아이콘을 좌우 반전해 한 번 더 (병의 태그가 왼쪽으로) — 피벗(발)은 같은 y=650 에 남는다.
		// 그 옆에 발을 축으로 천천히 도는 것 하나 (origin = 피벗), 그리고 좌상단 기준(origin 0,0)으로 도는 단색 사각형.
		if (const AtlasFrame* frame = animation->GetCurrentFrame())
		{
			spriteBatch->Draw(sheet, *frame, { 780.f, 650.f }, 0.6f, 0xFFFFFFFF, 0.f, SpriteEffects::FlipHorizontally);
			spriteBatch->Draw(sheet, *frame, { 560.f, 300.f }, 0.4f, 0xFFFFFFFF, timer->GetTotalTime() * 1.5f);
		}
		spriteBatch->Draw(textureManager->GetWhite(), { 700.f, 300.f, 40.f, 40.f }, nullptr, 0xFF00FF00, timer->GetTotalTime() * 1.5f, { 0.f, 0.f }, 0.f);
		spriteBatch->End();

		// 9단계 : 스트레스 데모. 전부 같은 시트이므로 Texture 정렬이면 드로우콜은 ceil(N / kMaxBatchSize) 개다.
		// BackToFront 는 depth = 발 y 라 아래쪽이 위쪽을 가린다 (겹친 아이콘에서 확인). 같은 depth 끼리는 stable_sort 라 순서가 흔들리지 않는다.
		{
			spriteBatch->Begin(stressSort, BlendMode::AlphaBlend, SamplerMode::PointClamp);
			const std::vector<const AtlasFrame*> frames = atlas->GetFrames("item_");
			if (frames.size() >= 12)
			{
				// [y-sort 검증 쌍] 일부러 "앞(발 y 가 큰 것)" 을 먼저 그린다. Deferred 면 나중에 그린 뒤쪽 아이콘이 위를 덮고(틀린 겹침),
				// BackToFront 면 depth(발 y) 순으로 다시 정렬되어 앞쪽 아이콘이 위에 온다. S 키로 번갈아 보면 겹침이 뒤집힌다.
				spriteBatch->Draw(sheet, *frames[0],  { 1100.f, 240.f }, 0.5f);   // 물약, 발 y = 240 (앞)
				spriteBatch->Draw(sheet, *frames[11], { 1150.f, 190.f }, 0.5f);   // 발 y = 190 (뒤) — 나중에 그려 Deferred 에서는 위에 온다
				for (const StressSprite& s : stress)
					spriteBatch->Draw(sheet, *frames[s.frame % frames.size()], s.position, 0.25f, 0xFFFFFFFF, s.rotation);
			}
			spriteBatch->End();
		}
	}
}

#pragma once
#include "pch.h"

// 전역 변수 핸들, 윈도우 사이즈등을 받아오기 위한 클래스
class Settings
{
public:
	static Settings& Get()
	{
		static Settings instance;
		return instance;
	}

	HWND GetWindowHandle() { return handle; }
	void SetWindowHandle(HWND handle) { this->handle = handle; }

	// width / height 는 "현재 백버퍼 크기(물리 픽셀)" 라는 의미다.
	// 창 생성 직후 MyWindows 가 클라이언트 크기를 넣어주고, 이후에는 Execute::Update() 가
	// Graphics::Resize() 에 성공했을 때만 갱신한다.
	// 주의 : 게임이 그리는 좌표계는 아래의 "논리 해상도"(GetLogicalWidth/Height) 다. 이 둘을 혼동하지 말 것.
	//        물리 크기는 창을 따라 수시로 바뀌고, 논리 해상도는 게임이 정한 값으로 고정이다.
	//        새 코드(스프라이트, UI)는 물리 픽셀을 직접 쓰면 안 되고 논리 좌표로 써야 한다.
	float GetWidth() { return width; }
	void SetWidth(float width) { this->width = width; }

	float GetHeight() { return height; }
	void SetHeight(float height) { this->height = height; }

	// ---- 논리 해상도 (가상 해상도) ----
	// 게임 월드/UI 가 그려지는 고정 좌표계. 창 크기와 무관하게 항상 이 값이다.
	// [왜 물리 픽셀 대신 논리 해상도로 그리는가]
	//   스프라이트 좌표를 물리 픽셀로 쓰면 창 크기가 바뀔 때마다 모든 좌표가 달라진다.
	//   논리 해상도로 쓰면 좌표가 절대 바뀌지 않고, 물리 크기로의 매핑은 뷰포트(D3D11_Viewport::Letterbox) 가 혼자 떠맡는다.
	//   직교 투영 행렬도 이 값으로 "한 번만" 만들면 되므로 후속 SpriteBatch 는 리사이즈를 전혀 몰라도 된다.
	// 물리 크기(width/height)와 종횡비가 다르면 레터박스(위아래 바)/필러박스(좌우 바)가 생긴다.
	// 기본값 1280x720 (16:9). 필요하면 Execute 생성자에서 바꾼다.
	// (3단계의 "DPI 논리 단위(96 DPI 기준)" 와는 다른 개념이다 — 아래 DPI 절 참고)
	uint GetLogicalWidth()  const { return logicalWidth; }
	uint GetLogicalHeight() const { return logicalHeight; }
	// 실행 중에 논리 해상도를 바꾸려면 이 함수가 아니라 Graphics::SetLogicalResolution() 을 쓸 것.
	// 이 함수는 값만 바꿀 뿐 뷰포트 재계산과 투영 행렬(SpriteBatch) 갱신을 하지 않는다. Graphics 가 내부에서 호출한다.
	// (Settings 는 Graphics 를 모르므로 반대 방향으로 막을 수 없다 — 규칙으로만 지킨다)
	void SetLogicalResolution(uint width, uint height) { logicalWidth = width; logicalHeight = height; }
	float GetLogicalAspectRatio() const { return static_cast<float>(logicalWidth) / logicalHeight; }

	// 논리 해상도 변경 요청. 리사이즈 요청과 같은 패턴 — WndProc(F1/F2/F3 데모)은 Graphics 에 닿을 수 없으므로 여기 적어 두고,
	// Execute::Update() 가 꺼내서 Graphics::SetLogicalResolution() 을 부른다. 마지막 값 하나만 남는다.
	void RequestLogicalResolution(uint width, uint height)
	{
		pendingLogicalWidth = width;
		pendingLogicalHeight = height;
		logicalResolutionPending = true;
	}

	bool ConsumeLogicalResolutionRequest(uint& outWidth, uint& outHeight)
	{
		if (!logicalResolutionPending) return false;
		outWidth = pendingLogicalWidth;
		outHeight = pendingLogicalHeight;
		logicalResolutionPending = false;
		return true;
	}

	// ---- 데모 키 요청 (9단계 스트레스 데모. WndProc 이 기록, Execute 가 소비) ----
	// +/- : 스트레스 스프라이트 수 단계 증감, S : 정렬 모드 순환. 논리 해상도 요청과 같은 "요청 → 루프에서 소비" 패턴.
	// 입력 시스템이 생기면 전부 그쪽으로 간다 — 키마다 플래그를 늘리는 것은 그때까지의 임시 통로다.
	void RequestStressStep(int direction) { stressStep += direction; }          // +1 / -1. 여러 번 눌리면 합산
	int  ConsumeStressStep() { const int v = stressStep; stressStep = 0; return v; }
	void RequestSortModeCycle() { ++sortModeCycle; }
	int  ConsumeSortModeCycle() { const int v = sortModeCycle; sortModeCycle = 0; return v; }

	// ---- 데모 장면 요청 (10단계. WndProc(메뉴 WM_COMMAND / 숫자키) 이 기록, Execute 가 소비) ----
	// 논리 해상도 요청과 같은 꼴 — 마지막 값 하나만 남는다 (메뉴를 연속으로 눌러도 마지막 장면만 의미 있다).
	// int 로 받는 이유 : Settings.h 가 DemoScene.h 를 몰라도 되게. 범위 검사는 기록하는 쪽(WndProc)과 소비하는 쪽(Execute) 이 한다.
	void RequestScene(int scene) { pendingScene = scene; scenePending = true; }
	bool ConsumeSceneRequest(int& outScene)
	{
		if (!scenePending) return false;
		outScene = pendingScene;
		scenePending = false;
		return true;
	}

	// ---- 창 상태 (WndProc 이 기록, Execute 가 소비) ----
	// [왜 WndProc 이 Graphics::Resize() 를 직접 부르지 않는가]
	//   1. WndProc 은 자유 함수라 Graphics 인스턴스에 닿을 방법이 없다. 이미 Settings 가 창 <-> 프레임워크 사이의
	//      전달 통로 역할을 하고 있으므로 그 통로를 그대로 쓴다.
	//   2. 테두리 드래그 중에는 WM_SIZE 가 초당 수십 번 온다. 메시지마다 스왑체인을 재생성하면 심하게 버벅인다.
	//      요청을 "마지막 값 하나" 로 덮어써 두고 메인 루프에서 한 번만 처리하면 자연히 병합된다.
	//   3. 메시지 핸들러 안에서 GPU 리소스를 재생성하면 재진입 문제가 생길 수 있다. 루프에서 처리하면 Render() 와 절대 겹치지 않는다.

	// 리사이즈 요청. WM_SIZE 가 올 때마다 덮어쓰므로 드래그 중 수십 번 와도 마지막 크기 하나만 남는다.
	void RequestResize(uint width, uint height)
	{
		pendingWidth = width;
		pendingHeight = height;
		resizePending = true;
	}

	// 요청이 있으면 꺼내고 true. Execute::Update() 에서 매 프레임 호출.
	bool ConsumeResizeRequest(uint& outWidth, uint& outHeight)
	{
		if (!resizePending) return false;
		outWidth = pendingWidth;
		outHeight = pendingHeight;
		resizePending = false;
		return true;
	}

	// 테두리 드래그 중인가 (WM_ENTERSIZEMOVE ~ WM_EXITSIZEMOVE). true 동안은 리사이즈 요청을 쌓아만 두고 처리하지 않는다.
	bool IsSizing() const { return sizing; }
	void SetSizing(bool value) { sizing = value; }

	// 최소화 상태인가. true 동안은 렌더링을 건너뛴다 (클라이언트 영역이 0x0 이라 그릴 곳이 없다).
	bool IsMinimized() const { return minimized; }
	void SetMinimized(bool value) { minimized = value; }

	// ---- DPI (WndProc 이 기록, 후속 작업(UI/텍스트)이 소비) ----
	// 현재 창이 위치한 모니터의 DPI 와 배율. 96 DPI = 100%, 120 = 125%, 144 = 150%, 192 = 200%.
	// 렌더링(백버퍼 크기) 자체는 DPI 를 쓰지 않는다 — 위의 width/height 가 이미 물리 픽셀이라 픽셀 수와 종횡비만 있으면 된다.
	// DPI 배율값이 실제로 필요해지는 건 "버튼이 모니터마다 물리적으로 같은 크기여야 한다" 같은 UI 단계라서,
	// 이 단계(3단계)는 값을 보관까지만 한다.
	// 주의: 4단계의 "논리 해상도(가상 해상도)" 와 이 "DPI 논리 단위(96 DPI 기준)" 는 다른 개념이다.
	UINT  GetDpi()      const { return dpi; }
	float GetDpiScale() const { return static_cast<float>(dpi) / USER_DEFAULT_SCREEN_DPI; }
	void  SetDpi(UINT value)  { dpi = value; }

private:
	HWND handle = nullptr;
	float width = 0.f;
	float height = 0.f;

	uint logicalWidth = 1280;
	uint logicalHeight = 720;

	bool resizePending = false;
	uint pendingWidth = 0;
	uint pendingHeight = 0;
	bool logicalResolutionPending = false;
	uint pendingLogicalWidth = 0;
	uint pendingLogicalHeight = 0;
	bool sizing = false;
	bool minimized = false;
	int stressStep = 0;
	int sortModeCycle = 0;
	bool scenePending = false;
	int  pendingScene = 0;
	UINT dpi = USER_DEFAULT_SCREEN_DPI;   // 96. 창 생성 직후 MyWindows::Create() 가 실제 값으로 덮어쓴다.
};
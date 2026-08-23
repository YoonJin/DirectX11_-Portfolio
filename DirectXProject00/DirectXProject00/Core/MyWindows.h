/*
* 인스턴스와 핸들
*
* 운영체제는 멀티 태스킹 multi tasking 운영체제이다.
* 같은 프로그램의 데이터를 운영체제에서 구별하는 방법은 ? 인스턴스(instance)와 핸들(handle)이 있다.
* 인스턴스와 핸들의 실체 ? 값
*
* 인스턴스의 데이터형 : HINSTANCE       == PVOID == void* : 4바이트의 양의 정수값
* 핸들의 데이터형     : HWND            == PVOID == void* : 4바이트의 양의 정수값
*
* [ 인스턴스란? ]
*
* - 응용프로그램의 아이디
* - 같은 종류의 프로그램은 같은 인스턴스를 가진다.
*
* [ 핸들이란? ]
*
* - 운영체제에서 할당한 자원의 아이디
* - (윈도우, 펜, 브러쉬 등)
*
*  [ Resource? ]
*
* - 각종 하드웨어 장치 또는 저장장치(메모리, 하드 등)에 들어있는 데이터 자료
*/

#pragma once
#include "pch.h"
#include "DemoScene.h"   // 메뉴 항목 / WM_COMMAND ID (10-A). pch.h 에는 넣지 않는다 — 이 헤더와 Execute.h 만 본다

// 전역함수 전역변수가 어디에 포함 되어 있는지 확인하기 위해 namespace 기재
namespace MyWindows
{
	// [static 이 아니라 inline 인 이유]
	// 헤더의 static 변수는 이 헤더를 include 하는 번역 단위(.cpp)마다 별도의 사본이 생긴다.
	// 지금은 MyWindows.cpp 만 include 하지만, 다른 .cpp 가 g_hWnd 를 참조하기 시작하면 서로 다른 변수를 보게 된다.
	// C++17 의 inline 변수는 프로그램 전체에서 하나의 실체만 갖도록 링커가 합쳐준다.
	inline HINSTANCE g_Instance = nullptr;   // 전역 변수 인스턴스.
	inline HWND g_hWnd = nullptr;            // 전역 변수 핸들


	//  함수: WndProc(HWND, UINT, WPARAM, LPARAM) 
	//
	//  용도: OS 에서 전달되는 주 창의 메시지를 처리한다.
	//
	//  WM_CLOSE         - 윈도우가 닫히기전 메시지가 전달된다.
	//  WM_DESTROY       - 윈도우가 소멸되고 있다는 것을 알려주기 위해 전달된다.
	//  WM_SIZE          - 클라이언트 영역 크기가 바뀌었다. (드래그, 최대화/복원, 최소화, SetWindowPos ...)
	//  WM_ENTERSIZEMOVE - 사용자가 테두리/타이틀바를 잡아 크기 조절/이동 모달 루프에 들어갔다.
	//  WM_EXITSIZEMOVE  - 그 모달 루프에서 빠져나왔다.
	//  WM_DPICHANGED    - 창이 배율이 다른 모니터로 옮겨졌거나 사용자가 배율 설정을 바꿨다. (Per-Monitor v1/v2 인식일 때만 온다)
	//  WM_KEYDOWN       - (데모) F1/F2/F3 으로 논리 해상도를 바꿔 레터박스 배치가 달라지는 것을 보여준다. +/- 스트레스 스프라이트 수, S 정렬 모드 (9단계). 0~8 데모 장면 (10-A).
	//  WM_COMMAND       - (데모) "데모(D)" 메뉴 항목 클릭. 숫자키와 같은 Settings::RequestScene() 을 부른다 (10-A).
	//
	//  [DPI 경로] WM_DPICHANGED -> Settings::SetDpi() + SetWindowPos(권장 사각형) -> WM_SIZE -> (위의 리사이즈 경로)
	//  [리사이즈 경로] WM_SIZE -> Settings::RequestResize() -> (메인 루프) Execute::Update() -> Graphics::Resize()
	//  [논리 해상도 경로] WM_KEYDOWN -> Settings::RequestLogicalResolution() -> (메인 루프) Execute::Update() -> Graphics::SetLogicalResolution()
	//  [장면 경로] WM_COMMAND(메뉴) / WM_KEYDOWN(0~8) -> Settings::RequestScene() -> (메인 루프) Execute::Update() -> 장면 전환 + CheckMenuRadioItem
	//  여기서 Graphics 를 직접 건드리지 않는 이유는 Settings.h 의 "창 상태" 절에 적어두었다.
	//
	inline LRESULT CALLBACK WndProc
	(
		HWND handle,
		UINT message,
		WPARAM wParam,
		LPARAM lParam
	)
	{
		switch (message)
		{
		case WM_CLOSE:
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_SIZE:
			// wParam : SIZE_MINIMIZED / SIZE_MAXIMIZED / SIZE_RESTORED ...
			// lParam : 클라이언트 영역 크기 (LOWORD = width, HIWORD = height)
			if (wParam == SIZE_MINIMIZED)
			{
				// 최소화 시 클라이언트 크기는 0x0 이다. 0 크기 ResizeBuffers 는 무효이므로 요청하지 않는다.
				Settings::Get().SetMinimized(true);
			}
			else
			{
				// 최대화 버튼이나 SetWindowPos 로 오는 WM_SIZE 는 WM_ENTERSIZEMOVE 없이 단독으로 온다.
				// 그래서 여기서도 요청을 넣어야 한다. 드래그 중이면 IsSizing() 이 true 라 루프에서 처리가 미뤄질 뿐이다.
				Settings::Get().SetMinimized(false);
				Settings::Get().RequestResize(LOWORD(lParam), HIWORD(lParam));
			}
			break;

		case WM_ENTERSIZEMOVE:
			// 사용자가 테두리/타이틀바를 잡았다. 이때부터 WM_EXITSIZEMOVE 까지 Windows 는 모달 루프에 들어가고
			// 우리 메인 루프(PeekMessage 루프)는 돌지 않는다. 드래그 중 WM_SIZE 폭풍은 RequestResize 가 마지막 값으로 덮어쓴다.
			Settings::Get().SetSizing(true);
			break;

		case WM_EXITSIZEMOVE:
			// 드래그 종료. 최종 클라이언트 크기로 요청을 한 번 더 넣어 확정한다.
			// (이동만 하고 크기가 안 바뀐 경우에도 요청이 들어가지만, Resize() 의 "같은 크기면 무시" 가 걸러준다)
			Settings::Get().SetSizing(false);
			{
				RECT rect{};                                  // GetClientRect 실패 시 0x0 -> Resize() 의 0 크기 필터가 흡수한다
				GetClientRect(handle, &rect);
				Settings::Get().RequestResize(
					static_cast<uint>(rect.right - rect.left),
					static_cast<uint>(rect.bottom - rect.top));
			}
			break;

		case WM_COMMAND:
			// [데모 메뉴 (10-A)] HIWORD(wParam) == 0 이면 메뉴 항목, 1 이면 가속키, 그 외는 자식 컨트롤 통지 — 이 창은 메뉴뿐이지만 규칙대로 거른다.
			// 메뉴 클릭도 키와 똑같이 Settings 에 요청만 적는다. WndProc 은 Graphics/Execute 에 닿지 않는다는 2단계 규칙 그대로.
			// 현재 장면의 라디오 체크 표시는 Execute 가 실제로 장면을 바꾼 뒤에 찍는다 (여기서 찍으면 요청이 버려졌을 때 표시와 실제가 어긋난다).
			if (HIWORD(wParam) == 0
				&& LOWORD(wParam) >= kSceneMenuIdBase
				&& LOWORD(wParam) < kSceneMenuIdBase + static_cast<UINT>(DemoScene::Count))
			{
				Settings::Get().RequestScene(static_cast<int>(LOWORD(wParam) - kSceneMenuIdBase));
				return 0;
			}
			return DefWindowProc(handle, message, wParam, lParam);

		case WM_DPICHANGED:
		{
			// 창이 배율이 다른 모니터로 이동했다 (또는 사용자가 실행 중 배율 설정을 바꿨다).
			//   wParam : HIWORD/LOWORD 모두 새 DPI (X/Y 가 같다고 보장됨)
			//   lParam : OS 가 계산해준 "권장 창 사각형" (새 DPI 에서 같은 DPI 논리 크기를 유지하는 크기/위치)
			// 권장 사각형을 그대로 적용하는 것이 표준 처리다. 직접 계산하면 모니터 경계에서 창이 커지며 다시 이전
			// 모니터로 걸쳐 들어가 WM_DPICHANGED 가 반복되는 "창이 튀는" 버그가 난다.
			// SetWindowPos 가 WM_SIZE 를 발생시키므로 백버퍼 재생성은 기존 리사이즈 경로가 처리한다.
			// 이 메시지는 Per-Monitor 인식(v1/v2)일 때만 오므로, 여기 중단점을 걸고 모니터 간 이동을 해보면 매니페스트 적용 여부가
			// 확인된다. v2 인지 여부는 타이틀바/테두리까지 배율에 맞게 커지는지로 구분한다 (v1 은 타이틀바가 작게 남는다).
			const UINT newDpi = HIWORD(wParam);
			Settings::Get().SetDpi(newDpi);

			const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
			SetWindowPos(handle, nullptr,
				suggested->left, suggested->top,
				suggested->right - suggested->left,
				suggested->bottom - suggested->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}

		case WM_KEYDOWN:
		{
			// [논리 해상도 전환 데모 — 포트폴리오 영상용]
			// F1 = 1280x720 (16:9), F2 = 800x600 (4:3), F3 = 1920x1080 (16:9).
			// 리사이즈 요청과 같은 패턴으로 Settings 에 "논리 해상도 변경 요청" 만 적어 둔다.
			// Execute::Update() 가 꺼내서 Graphics::SetLogicalResolution() 을 부르면 뷰포트 재계산 + 논리 해상도 리스너(SpriteBatch 투영) 통지가 일어난다.
			// (4단계에서는 Settings::SetLogicalResolution() 을 직접 부르고 같은 크기로 RequestResize 를 넣는 우회를 썼다.
			//  그 방식은 투영 행렬을 가진 쪽에 통지가 가지 않아 5단계에서 이 채널로 바꿨다)
			// 16:9 창에서 F2 를 누르면 좌우에 필러박스가 생기고, F1/F3 은 종횡비가 같아 바 배치가 같다(뷰포트 크기 동일, 논리 좌표 밀도만 다름).
			// 입력 시스템이 생기면 그쪽으로 옮긴다. 여기서는 WndProc 이 Settings 에만 쓴다는 규칙을 그대로 지킨다.
			uint logicalWidth = 0, logicalHeight = 0;
			switch (wParam)
			{
			case VK_F1: logicalWidth = 1280; logicalHeight = 720;  break;
			case VK_F2: logicalWidth = 800;  logicalHeight = 600;  break;
			case VK_F3: logicalWidth = 1920; logicalHeight = 1080; break;
			// [9단계 스트레스 데모] 같은 패턴으로 요청만 적는다. VK_OEM_PLUS/MINUS 는 메인 키보드의 =/+ 와 -/_ (숫자 패드는 VK_ADD/VK_SUBTRACT).
			case VK_OEM_PLUS:  case VK_ADD:      Settings::Get().RequestStressStep(+1); return 0;
			case VK_OEM_MINUS: case VK_SUBTRACT: Settings::Get().RequestStressStep(-1); return 0;
			case 'S':                            Settings::Get().RequestSortModeCycle(); return 0;
			// [10-A 데모 장면] 메인 키보드 0~9 (가상 키 코드 = ASCII '0'~'9'). 메뉴 클릭(WM_COMMAND)과 같은 통로. DemoScene::Count 이상은 무시.
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				if (static_cast<int>(wParam - '0') < static_cast<int>(DemoScene::Count))
					Settings::Get().RequestScene(static_cast<int>(wParam - '0'));
				return 0;
			default: return DefWindowProc(handle, message, wParam, lParam);
			}
			Settings::Get().RequestLogicalResolution(logicalWidth, logicalHeight);
			return 0;
		}

		default:
			return DefWindowProc(handle, message, wParam, lParam);
		}

		return 0;
	}


	//
	//  함수: Create()
	//
	//  용도: 윈도우 창 정보를 세팅 후, 해당 정보가 담긴 클래스를 등록합니다.
	//
	inline void Create(HINSTANCE hInstance, const UINT& width, const UINT& height)
	{
		g_Instance = hInstance;                       // Destroy() 의 UnregisterClass 가 쓴다. (이전에는 대입이 빠져 nullptr 이 넘어갔다)

		WNDCLASSEX wnd_class;	                      // WNDCLASS : 윈도우의 정보를 저장하기 위한 구조체

		wnd_class.cbSize = sizeof(WNDCLASSEX);               // 구조체의 크기 정보

		wnd_class.style = CS_HREDRAW | CS_VREDRAW;           // 윈도우 스타일
		wnd_class.lpfnWndProc = WndProc;                     // 윈도우 프로시져 (메시지 처리함수) 
		wnd_class.cbClsExtra = 0;                            // 클래스 여분의 메모리
		wnd_class.cbWndExtra = 0;                            // 윈도우 여분의 메모리
		wnd_class.hInstance = hInstance;                     // 인스턴스
		wnd_class.hIcon = LoadIcon(nullptr, IDI_ERROR);      // 아이콘
		wnd_class.hIconSm = LoadIcon(nullptr, IDI_ERROR);    // 작은 아이콘

		// GetStockObject : API 함수를 사용해서 개발에 필요한 다양한 자원을 운영체제로부터 제공받는다.
		wnd_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));               // 백그라운드 색상
		wnd_class.hCursor = LoadCursor(nullptr, IDC_ARROW);                                       // 커서 아이콘
		wnd_class.lpszClassName = L"D2D11Game";                                                   // 클래스 이름
		wnd_class.lpszMenuName = nullptr;                                                         // 클래스 메뉴 없음 — 데모 메뉴는 아래에서 창에 직접 붙인다 (10-A)

		// 윈도우 클래스 등록
		auto check = RegisterClassEx(&wnd_class);
		// check변수의 값이 true라면 문제없이 실행
		assert(check != 0);

		// [클라이언트 영역 크기 보정]
		// 인자 width/height 는 "클라이언트 영역(그림이 그려지는 안쪽)" 크기다.
		// 그런데 CreateWindowEx 에 넘기는 크기는 테두리와 타이틀바를 포함한 "창 전체" 크기라서,
		// 500 을 그대로 넘기면 클라이언트 영역은 약 484x461 이 된다.
		// AdjustWindowRectExForDpi 가 원하는 클라이언트 사각형 -> 창 전체 사각형 역산을 해준다.
		//
		// style / exStyle 을 변수로 뽑은 이유 : 역산 결과가 스타일에 따라 달라지므로
		// AdjustWindowRectExForDpi 와 CreateWindowExW 에 반드시 같은 값을 넘겨야 한다.
		const DWORD style = WS_OVERLAPPEDWINDOW;
		const DWORD exStyle = WS_EX_APPWINDOW;

		// [DPI 반영]
		// 인자 width/height 는 "DPI 논리 단위(96 DPI 기준)" 다. 150% 모니터에서 500 논리 = 750 물리 픽셀.
		// 프로세스가 Per-Monitor v2 (app.manifest) 라서 OS 가 창을 대신 확대해주지 않으므로, 우리가 직접 배율을 곱해야
		// 100% 모니터와 같은 물리적 크기로 보인다. (곱하지 않으면 150% 에서 창이 500 물리 픽셀로 작게 뜬다)
		// 창이 아직 없으므로 "주 모니터(시스템) DPI" 로 먼저 크기를 잡는다.
		// 창이 다른 배율의 모니터에 뜨면 WM_DPICHANGED 가 와서 거기서 보정된다.
		//
		// AdjustWindowRectEx 가 아니라 ...ForDpi 를 쓰는 이유 : 전자는 "시스템 DPI 기준 테두리 두께" 를 쓰므로
		// PMv2 프로세스에서는 틀린 값을 줄 수 있다. 후자는 넘겨준 dpi 기준으로 타이틀바/테두리 두께를 계산한다.
		const UINT dpi = GetDpiForSystem();
		const float scale = static_cast<float>(dpi) / USER_DEFAULT_SCREEN_DPI;   // USER_DEFAULT_SCREEN_DPI == 96

		// [데모 메뉴 (10-A)] 리소스 파일(.rc) 없이 코드로 만든다 — .rc 를 붙이면 .vcxproj 설정이 늘고 UTF-8 리소스 컴파일 이슈가 생긴다.
		// 항목 텍스트와 ID 는 DemoScene.h 의 kDemoScenes / kSceneMenuIdBase 가 단일 출처다.
		// 창에 붙인 메뉴는 DestroyWindow 가 같이 지운다 — 따로 DestroyMenu 하지 않는다 (두 번 지우면 에러). 창 생성이 실패하면 메뉴가 새지만 그때는 assert 로 멈춘다.
		HMENU sceneMenu = CreatePopupMenu();
		assert(sceneMenu != nullptr);
		for (int i = 0; i < static_cast<int>(DemoScene::Count); ++i)
			AppendMenuW(sceneMenu, MF_STRING, static_cast<UINT_PTR>(kSceneMenuIdBase) + i, kDemoScenes[i].menuLabel);
		HMENU menuBar = CreateMenu();
		assert(menuBar != nullptr);
		AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(sceneMenu), L"데모(&D)");
		// 초기 장면(0) 에 라디오 체크. 이후의 체크 이동은 Execute 가 장면을 바꿀 때 같은 호출로 한다 (Execute → 창 방향은 허용, 반대만 금지).
		CheckMenuRadioItem(sceneMenu, kSceneMenuIdBase, kSceneMenuIdBase + static_cast<UINT>(DemoScene::Count) - 1, kSceneMenuIdBase, MF_BYCOMMAND);

		RECT rect = { 0, 0,
			static_cast<LONG>(width  * scale + 0.5f),    // +0.5 : 반올림 (125% 에서 500*1.25 = 625 처럼 정수가 아닐 수 있다)
			static_cast<LONG>(height * scale + 0.5f) };
		// bMenu = TRUE : 메뉴 바 높이까지 창 전체 크기에 더한다. FALSE 인 채로 메뉴를 붙이면 메뉴 높이만큼 클라이언트가 줄어
		// 3단계 검증 수치(150% 에서 750×750)가 어긋나고 레터박스 두께가 달라진다. (메뉴가 두 줄로 접히는 폭에서는 여전히 어긋날 수 있지만 항목이 하나라 해당 없음)
		auto adjusted = AdjustWindowRectExForDpi(&rect, style, TRUE, exStyle, dpi);
		assert(adjusted != 0);
		// Release 에서는 assert 가 사라진다. 실패하면 rect 가 클라이언트 크기 그대로 남아 창이 조금 작게(테두리만큼) 뜰 뿐
		// 동작에는 지장이 없으므로 별도 복구 경로는 두지 않는다.

		// Window창 생성 (Unicode환경)
		g_hWnd = CreateWindowExW
		(
			exStyle,
			L"D2D11Game",                   // 윈도우 클래스 이름 
			L"D2D11Game",                   // 타이틀 바에 띄울 이름 
			style,                          // 윈도우 스타일
			CW_USEDEFAULT,                  // 윈도우 화면 좌표 x 
			CW_USEDEFAULT,                  // 윈도우 화면 좌표 y
			rect.right - rect.left,         // 창 전체 가로 사이즈 (클라이언트 width 가 되도록 역산된 값)
			rect.bottom - rect.top,         // 창 전체 세로 사이즈
			nullptr,                        // 부모 윈도우
			menuBar,                        // 메뉴 핸들 (10-A 데모 메뉴. 창이 소유한다)
			hInstance,                      // 인스턴스 지정
			nullptr                         // 자식 윈도우를 생성하면 지정, 그렇지 않으면 NULL
		);

		assert(g_hWnd != nullptr);              // 핸들이 문제없이 지정이 되었는지 확인 

		// 창이 실제로 놓인 모니터의 DPI 를 읽어 보관한다. CW_USEDEFAULT 로 만든 창은 보통 주 모니터에 뜨므로
		// 위의 GetDpiForSystem() 과 같은 값이지만, 진실은 창 쪽에 있으므로 창 기준으로 다시 읽는다.
		// GetDpiForWindow 는 실패 시 0 을 돌려준다. 0 이 들어가면 GetDpiScale() 이 0 이 되어 후속 UI 스케일이 조용히 0 으로
		// 곱해지므로, 실패하면 위에서 구한 시스템 DPI 로 대체한다. 이후 변경은 WndProc 의 WM_DPICHANGED 가 갱신한다.
		const UINT windowDpi = GetDpiForWindow(g_hWnd);
		Settings::Get().SetDpi(windowDpi != 0 ? windowDpi : dpi);
	}

	inline void Show()
	{
		ShowWindow(g_hWnd, SW_NORMAL);
		UpdateWindow(g_hWnd);
	}

	inline bool Update()
	{
		MSG msg;
		// 메시지 비워준다.
		ZeroMemory(&msg, sizeof(MSG));

		// PeekMessage()는 메세지 큐에 메세지가 존재한다면 가져와서 MSG 구조체에 그 값을 저장하고 0이 아닌 값을 반환한다. 
		// 해당 함수는 메세지 큐가 비어있을 때 GetMessage() 처럼 무한정 기다리지 않고 바로 0을 리턴한다. 
		// 때문에 PeekMessage() 함수는 무한 대기에 빠지지 않고, 메세지 큐가 비었다면 다음과 같이 다른 작업을 해줄 수 있다. 
		// 해당 함수도 메세지를 읽어오면 GetMessage와 동일하게 메세지가 번역되어 메세지 처리 함수로 보내진다.
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);      // 키보드 입력이 들어왔을때의 처리정보 전달
			DispatchMessage(&msg);       // 메세지 전달 // 값을 반환받을 때 까지 대기
		}

		return msg.message != WM_QUIT;
	}

	inline void Destroy()
	{
		// 윈도우 해제
		DestroyWindow(g_hWnd);
		UnregisterClass(L"D2D11Game", g_Instance);
	}

	// 화면 넓이 사이즈 가져오기
	inline UINT GetWidth()
	{
		RECT rect;
		GetClientRect(g_hWnd, &rect);

		return static_cast<UINT>(rect.right - rect.left);
	}

	// 화면 높이 사이즈 가져오기
	inline UINT GetHeight()
	{
		RECT rect;
		GetClientRect(g_hWnd, &rect);

		return static_cast<UINT>(rect.bottom - rect.top);
	}
}

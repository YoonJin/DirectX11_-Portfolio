#include "pch.h"
#include "Core/MyWindows.h"
#include "Core/Execute.h"


// [WinMain의 역할]
// 1. 응용 프로그램 윈도우 구조체의 등록과 생성
// -> 화면에 출력할 윈도우의 정보들을 결정
// 
// 2. 메시지 루프 
// -> OS에서 프로그램에 전달하려하는 메시지가 있는지 지속적으로 확인한다.
// 3. 메시지 체크와 전달
// -> 메시지가 존재한다면 각 함수에 해당 내용을 전달

/*
*  [구조]
*
*   WinMain()
*   {
*      1. 윈도우 구조체 설정 및 등록
*      2. 윈도우 생성과 출력
*      3. 메시지 루프
*   }
*/

int APIENTRY WinMain
(
	HINSTANCE hInstance,
	HINSTANCE prevInstance,
	LPSTR lpszCmdParam,
	int nCmdShow
)
{
	// [COM 초기화]
	// 5단계의 WIC(PNG 로더) 팩토리는 COM 객체라 CoCreateInstance 전에 이 스레드에서 CoInitializeEx 가 되어 있어야 한다.
	// D3D11 객체들도 COM 인터페이스(IUnknown 상속, Release 로 해제)지만 D3D11CreateDevice 는 COM 런타임을 거치지 않으므로
	// CoInitialize 없이도 동작한다 — 그래서 4단계까지는 없어도 됐다. 이 차이가 헷갈리기 쉬워 여기 적어 둔다.
	// 짝인 CoUninitialize 는 Execute(→ 모든 WIC/D3D 객체) 가 삭제된 뒤에 부른다.
	// MULTITHREADED 는 WIC 에는 문제없지만, 메시지 펌프를 도는 UI 스레드는 관례적으로 STA 다. 나중에 IFileDialog / 드래그앤드롭 / OLE 을
	// 붙이면 MTA 에서는 동작하지 않으므로 그때 COINIT_APARTMENTTHREADED 로 바꿔야 한다.
	HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	CHECK(hr);

	MyWindows::Create(hInstance, 500, 500);
	MyWindows::Show();

	// Settings 클래스에 필요한 최소한의 정보를 넘겨준다.
	Settings::Get().SetWindowHandle(MyWindows::g_hWnd);
	Settings::Get().SetWidth(static_cast<float>(MyWindows::GetWidth()));
	Settings::Get().SetHeight(static_cast<float>(MyWindows::GetHeight()));

	// 실행에 관련된 클래스의 인스턴스를 만들어준다.
	Execute* execute = new Execute();

	while (MyWindows::Update())
	{
		execute->Update();
		execute->Render();
	}

	SAFE_DELETE(execute);

	MyWindows::Destroy();

	// CoInitializeEx 의 짝. COM 객체를 전부 놓은 뒤(Execute 삭제 후)에 불러야 한다.
	::CoUninitialize();
	return 0;
}
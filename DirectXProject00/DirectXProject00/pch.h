// pch.h: 미리 컴파일된 헤더 파일이다.
// 말 그대로 헤더를 미리 컴파일 해두는 것이다. 
// 프로그램을 만들다보면 어쩔 수 없이 프로그램의 몸집은 점점 불어난다. 
// 프로그램이 커지면 전처리기가 컴파일해야될 헤더도 엄청나게 많아진다.
// 그래서 자주변경되지 않는 긴 소스코드를 미리 컴파일하여, 컴파일 결과를 별도의 파일에 저장한다.
// 다시 똑같은 헤더를 컴파일 할때 처음부터 컴파일 하는 것이 아닌 미리 컴파일된 헤더파일을 사용해 컴파일 속도를 월등히 향상시켜 준다.

#pragma once

// Window
#include <Windows.h>
#include <assert.h>
#include <string>

// STL
#include <vector>
#include <memory>          // unique_ptr — 6단계 TextureManager/ShaderManager 가 리소스를 소유하는 방식
#include <unordered_map>   // 경로 → 리소스 캐시 (6단계)
#include <filesystem>      // Path.h — 에셋 경로 정규화 (C++17)

using namespace std;

// DirectX
#include <d3dcompiler.h>
#include <d3d11.h>
#include <d3d11_1.h>      // ID3D11DeviceContext1 (ClearView — 4단계 레터박스 영역 클리어). Windows 8+ 런타임.
#include <DirectXMath.h>
#include <wincodec.h>     // WIC (Windows Imaging Component) — PNG 디코딩. 5단계 SpriteBatch::LoadTexture 가 쓴다. COM 이라 CoInitializeEx 필요 (MyWindows.cpp).

using namespace DirectX;

// ComPtr : COM 객체 전용 스마트 포인터.
//   unique_ptr / shared_ptr 과 같은 RAII 개념이지만, delete 대신 Release() 를 호출하고
//   복사 시 AddRef() 를 호출한다. D3D 객체는 전부 COM 이므로 이걸로 수명을 관리한다.
//   - .Get()          : raw 포인터를 꺼낸다 (API 에 넘길 때)
//   - .GetAddressOf() : ID3D11XXX** 가 필요한 출력 인자에 넘길 때 (기존 값을 해제하지 않음)
//   - &ptr            : 출력 인자용이지만, 기존 값을 먼저 Release 한다. 재생성 시 의도적으로 사용.
//                       매 프레임 호출되는 API(OMSetRenderTargets 등)에 &ptr 을 넘기면 프레임마다
//                       객체가 해제되므로 반드시 GetAddressOf() 를 써야 한다.
//   - .Reset()        : 명시적으로 해제
//   - .As(&other)     : QueryInterface (다른 인터페이스로 변환)
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <functional>

// pragma comment : 라이브러리와 관련된 정보등을 포함 하기 위해 사용될 수 있다.
/*
   1. 헤더 파일:

	* 용도: 코드의 인터페이스를 제공합니다. 함수, 클래스, 타입 등의 선언을 포함한다.
	* 컴파일: 헤더 파일 자체는 컴파일되지 않지만, 헤더 파일을 포함하는 소스 파일은 컴파일된다.
	* 내용: 함수나 클래스의 선언, 매크로, 타입 정의, 템플릿 등을 포함할 수 있다.
			구현(즉, 실제 동작하는 코드)을 포함할 수도 있지만, 일반적으로 소스 파일(.c, .cpp)에 구현을 두는 것이 좋다.

	2. 라이브러리:

	* 용도: 이미 컴파일된 코드의 집합을 제공한다. 함수, 클래스 등의 실제 구현을 포함한다.
	* 컴파일: 라이브러리 내의 코드는 이미 컴파일된 상태이다.
	* 내용: 함수나 클래스의 실제 구현을 포함하며, 이러한 구현은 링크 과정에서 사용된다.
			따라서, 두 개체의 주요 차이점은 다음과 같다.

	- 헤더 파일은 선언과 인터페이스를 제공하는 반면, 라이브러리는 실제 구현을 제공한다.
	- 헤더 파일은 컴파일 시점에 다른 소스 파일에 포함되어 사용되는 반면, 라이브러리는 링크 시점에 실행 파일과 함께 결합된다.
*/
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")   // WIC (CLSID_WICImagingFactory 등의 GUID 정의가 여기 있다)

typedef unsigned int uint;

// Macro Function
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = nullptr; } }
#define SAFE_DELETE_ARRAY(p) { if (p) {delete[] (p); (p) = nullptr; } }

// I로 시작되는 DirectX의 컴객체(컴인터페이스) 같은 경우는 delete가 아닌 자체에 포함된 Release()함수로 메모리에서
// 해제를 해주어야 한다.
// ComPtr 이 소멸자에서 이것과 같은 일을 자동으로 해준다. 프레임워크의 COM 객체는 전부 ComPtr 로 관리하므로
// 아래 매크로는 개념 설명용으로 남겨둔다.
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = nullptr; } }

// p의 인스턴스가 제대로 들어갔다면 프로그램을 진행하며, 그게 아니라면 프로그램을 강제 종료시킨다.
#define CHECK(p) assert(SUCCEEDED(p))

// Framework
#include "Core/Settings.h"
#include "Core/Graphics.h"

#include "D3D11_Viewport.h"

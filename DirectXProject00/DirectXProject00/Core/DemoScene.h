#pragma once
#include "pch.h"   // UINT. 이 헤더 자체는 pch.h 에 들어가지 않는다 (아래 참고)

// [데모 장면 목록 (10-A)]
// 메뉴 항목 / 숫자키 / Execute 의 switch 가 전부 이 열거형 하나를 본다.
// 순서 = 메뉴 순서 = 구현한 순서(1~4단계 → 5~9단계). 이름은 메뉴에 그대로 찍히므로 "기능 이름 — 전 / 후" 꼴로 쓴다.
//
// [왜 Win32 메뉴인가]
//   프레임워크에 텍스트 렌더링이 없다(9단계 "하지 않는 것"). 비트맵 폰트를 넣는 대신 OS 가 공짜로 그려 주는 메뉴와 창 제목을
//   자막으로 쓴다. 시연 영상에서 메뉴 이름만 봐도 "무엇을 구현했는지" 가 보이는 것이 목적이다.
//
// [왜 pch.h 에 넣지 않는가]
//   이 헤더를 보는 곳은 MyWindows.h(메뉴 생성 / WM_COMMAND) 와 Execute.h(장면 switch) 둘뿐이다. pch.h 에 넣으면 장면 하나를
//   추가할 때마다 전체 리빌드가 된다. 두 곳에서 직접 include 한다.
//
// [토글 / 분할]
//   장면 3~8 은 화면을 좌/우로 나눠 왼쪽 = 적용 전, 오른쪽 = 적용 후를 나란히 그린다 (Execute::HalfOriginX — 뷰포트가 아니라 논리 좌표 오프셋).
//   장면 1·2 는 백버퍼/뷰포트 자체가 주제라 분할이 불가능하므로 스페이스 토글로 한다 (10-C). 이 세션(10-A)에서는 골격만 두고 장면 0 과 같게 그린다.
enum class DemoScene
{
	All,            // 0 : 9단계까지의 전체 데모 (지금 화면 그대로. 회귀 확인용)
	Resize,         // 1 : 창 리사이즈 — 백버퍼 고정(뭉개짐) / 재생성          [토글, 10-C]
	Letterbox,      // 2 : 논리 해상도 + 레터박스 — 늘어남 / 레터박스          [토글, 10-C]
	Premultiplied,  // 3 : 프리멀티플라이드 알파 — straight / premultiplied   [분할, 10-C]
	BlendModes,     // 4 : 렌더 상태 — Opaque / AlphaBlend (+ Additive)       [분할, 10-B]
	Samplers,       // 5 : 샘플러 — Point / Linear                            [분할, 10-B]
	Atlas,          // 6 : 아틀라스 + 애니메이션 — 개별 4장 / 시트 1장 순환    [분할, 10-B]
	YSort,          // 7 : y-sort — Deferred / BackToFront                     [분할, 10-B]
	Batching,       // 8 : 배칭 — 즉시 그리기 / 배칭 (드로우콜·ms)            [분할, 10-C]
	Count
};

// WM_COMMAND 의 wParam(LOWORD) = kSceneMenuIdBase + 장면 번호. 메뉴는 리소스(.rc) 없이 코드로 만들므로 ID 도 여기서 정한다.
// 1000 부터인 이유 : 0 / 1 / 2 … 는 IDOK·IDCANCEL 같은 표준 컨트롤 ID 와 겹친다 (시스템 메뉴의 SC_* 는 WM_SYSCOMMAND 로 따로 오므로 무관).
constexpr UINT kSceneMenuIdBase = 1000;

struct DemoSceneInfo
{
	const wchar_t* menuLabel;   // 메뉴 항목 텍스트. '&' 다음 글자가 Alt 단축키 (&0 → 메뉴가 열린 상태에서 0)
	const wchar_t* before;      // 창 제목의 "좌: …" (적용 전)
	const wchar_t* after;       // 창 제목의 "우: …" (적용 후)
};

// 메뉴 라벨과 제목 표시용 문자열. 인덱스 = DemoScene. (ASCII 가 아니어도 된다 — Win32 메뉴/제목은 wide 문자열이고 소스는 /utf-8 로 컴파일된다)
// inline 변수 : 헤더를 두 번역 단위가 include 해도 실체는 하나 (MyWindows.h 의 g_hWnd 와 같은 이유).
inline const DemoSceneInfo kDemoScenes[static_cast<int>(DemoScene::Count)] =
{
	{ L"&0 전체 데모 (9단계까지)",                 L"",                    L""                      },
	{ L"&1 창 리사이즈 — 백버퍼 고정 / 재생성",    L"백버퍼 고정(Blt)",    L"백버퍼 재생성"         },
	{ L"&2 논리 해상도 — 늘어남 / 레터박스",       L"뷰포트 = 창 전체",    L"레터박스"              },
	{ L"&3 프리멀티플라이드 알파 — straight / pre", L"straight 알파",       L"premultiplied"         },
	{ L"&4 렌더 상태 — Opaque / AlphaBlend",       L"Opaque",              L"AlphaBlend + Additive" },
	{ L"&5 샘플러 — Point / Linear",               L"PointClamp",          L"LinearClamp"           },
	{ L"&6 아틀라스 — 개별 4장 / 시트 1장",        L"개별 텍스처 4장",    L"아틀라스 + 애니메이션" },
	{ L"&7 y-sort — Deferred / BackToFront",       L"Deferred",            L"BackToFront"           },
	{ L"&8 배칭 — 즉시 그리기 / 배칭",             L"드로우 1개 = 콜 1개", L"큐 → flush"            },
};

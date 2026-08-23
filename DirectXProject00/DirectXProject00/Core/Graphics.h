#pragma once
#include "D3D11_Viewport.h"

class Graphics final
{
public:
	Graphics();
	~Graphics();

public:
	/// <summary>
/// 더블 버퍼링에서 백버퍼로 사용될 버퍼를 생성 및 설정하는 함수.
/// </summary>
	void CreateDeviceAndSwapChain();
	/// <summary>
/// * 백버퍼에 대한 접근 방법이 저장된 리소스 View를 만들어준다. (최초 1회)
/// * [RenderTarget] : "렌더 타겟"은 그래픽스 파이프라인의 출력이 저장되는 메모리 영역을 나타낸다.
///                    일반적으로 이것은 화면에 직접 표시되는 백 버퍼일 것이다.
///
/// * [View] : 특정 데이터나 객체를 시각적화를 나타내는 데 사용된다.
///            이것은 그래픽스 데이터를 어떻게 렌더링 할 것인지 정의하는 특정 "방법" 또는 "각도"
///            를 나타낼 수 있다.
///
/// * [RenderTargetView] : DirectX에서 렌더링 목적지, 즉 그래픽스 파이프라인 출력의 최종
///                        목적지를 나타내는 객체이다.
///                        그래픽스 데이터(픽셀)가 렌더링 되는 '화면' 또는 '표면'을 표현한다.
/// </summary>
	void CreateRenderTargetView(const uint& width, const uint& height);

	/// <summary>
	/// 백버퍼 크기를 바꾼다. (2회차 이후 — 창 크기 변경 시)
	/// 백버퍼에 종속된 리소스(RTV)를 모두 놓은 뒤 스왑체인 버퍼를 재할당하고, RTV 와 뷰포트를 다시 만든다.
	/// 끝나면 AddResizeListener 로 등록된 콜백들에게 새 크기를 통지한다.
	/// 0 크기(최소화)이거나 현재 크기와 같으면 아무것도 하지 않는다.
	/// (논리 해상도 변경은 SetLogicalResolution() 의 일이다 — 5단계에서 분리)
	/// 반환값 : 백버퍼가 요청한 크기로 준비되어 있으면 true (무시된 경우 포함). ResizeBuffers / RTV 생성이 실패하면 false.
	///          CHECK 는 Release 에서 사라지므로, 호출자는 false 를 보고 재시도하거나 렌더를 건너뛰어야 한다.
	/// </summary>
	bool Resize(const uint& width, const uint& height);

	/// <summary>
	/// 렌더링 파이프라인의 마지막 단계인 뷰포트 변환에 대한 정보를 세팅하는 함수
	/// 3D 장면을 2D로 투영(변환) 할때 해당 이미지가 어떻게 그려질 것인지를 결정한다.
	/// 예를 들면, 전체 화면을 채울 것인지, 아니면 화면의 일부만 그릴 것인지,
	/// 어떤 비율로 그릴 것인지 등을 결정한다.
	///
	/// width/height 는 백버퍼(물리) 크기다. 4단계부터 뷰포트는 백버퍼 전체가 아니라
	/// Settings 의 논리 해상도 종횡비를 유지하며 그 안에 최대 크기로 중앙 정렬된 영역(레터박스)이 된다.
	/// Resize() 가 매번 호출하므로 리사이즈 시 자동으로 갱신된다.
	/// </summary>
	void SetViewport(const uint& width, const uint& height);

	ID3D11Device* GetDevice() { return _device.Get(); }
	ID3D11DeviceContext* GetDeviceContext() { return _deviceContext.Get(); }

	// 그릴 수 있는 백버퍼 RTV 가 있는가. Release 빌드에서 Resize() 가 실패한 직후에만 false 가 된다.
	bool HasRenderTarget() const { return _renderTargetView != nullptr; }

	uint GetBackBufferWidth()  const { return _backBufferWidth; }
	uint GetBackBufferHeight() const { return _backBufferHeight; }
	D3D_FEATURE_LEVEL GetFeatureLevel() const { return _featureLevel; }

	// 현재 뷰포트(물리 픽셀) = 레터박스 안쪽의 "게임 영역". 화면 크기에 의존하는 쪽(테스트 패턴, 마우스 좌표 변환)이 매 프레임 읽어 간다.
	// 리사이즈 이벤트를 따로 구독하지 않아도 되도록 "읽기"만 열어 둔다. 종횡비가 다르면 백버퍼 전체보다 작다.
	const D3D11_Viewport& GetViewport() const { return _viewport; }

	// ---- 리사이즈 이벤트 ----
	// 백버퍼 크기에 종속된 리소스(깊이 버퍼, 투영 행렬, 포스트 프로세스 RT 등)는 Graphics 가 알지 못한다.
	// 대신 여기에 콜백을 등록해두면 Resize() 가 끝난 뒤 새 크기를 넘겨 호출해준다.
	// Graphics → 상위 시스템 방향으로 의존이 생기지 않게 하기 위한 구조다.
	// (Graphics 가 깊이 버퍼나 SpriteBatch 를 직접 알면, 화면 크기 종속 리소스가 늘어날 때마다
	//  Graphics 를 고쳐야 한다. 구독 구조면 새 리소스가 스스로 등록하면 끝이다.)
	using ResizeCallback = std::function<void(uint width, uint height)>;
	uint AddResizeListener(ResizeCallback callback);   // 반환값 : 해제용 id
	void RemoveResizeListener(uint id);

	// ---- 논리 해상도 변경 이벤트 (리사이즈 이벤트와 별개 채널) ----
	// 논리 해상도를 바꾼다. Settings 에 값을 쓰고, 뷰포트(레터박스)를 다시 계산하고, 구독자에게 통지한다.
	// 리사이즈 리스너와 별개인 이유 : 리사이즈는 "백버퍼 크기 종속 리소스(깊이 버퍼, 화면 크기 RT)" 용이고,
	// 이건 "논리 좌표계 종속 리소스(SpriteBatch 의 직교 투영 행렬)" 용이다. 백버퍼 크기는 그대로이므로 리사이즈 리스너는 부르지 않는다.
	// 이 채널 덕분에 SpriteBatch 의 "투영은 논리 해상도로 한 번만" 이 "논리 해상도가 바뀔 때만" 으로 정확해진다.
	// (4단계의 "같은 크기로 RequestResize 를 넣어 뷰포트만 재계산" 우회는 5단계에서 이것으로 대체됐다)
	void SetLogicalResolution(uint width, uint height);
	using LogicalResolutionCallback = std::function<void(uint width, uint height)>;
	uint AddLogicalResolutionListener(LogicalResolutionCallback callback);   // 반환값 : 해제용 id
	void RemoveLogicalResolutionListener(uint id);

	void RenderBegin();
	void RenderEnd();

private:
	// 백버퍼에서 RTV 를 만드는 공통 경로. 최초 생성(CreateRenderTargetView)과 Resize 가 함께 쓴다.
	bool CreateBackBufferResources(const uint& width, const uint& height);   // 실패 시 false (크기 필드 미갱신)

private:
	// [멤버 선언 순서 = 해제 순서의 역순]
	// C++ 은 멤버를 선언의 역순으로 파괴한다. ComPtr 은 소멸자에서 Release() 를 호출하므로,
	// 아래 순서대로 선언하면 RTV → 스왑체인 → 컨텍스트 → 디바이스 순으로 해제된다.
	// 디바이스가 다른 모든 객체보다 오래 살아야 하므로 디바이스를 맨 위에 둔다.

	// Device & SwapChain
	// * Device : GPU에 대한 접근과 제어가 가능하다.
	//            GPU에 관련된 리소스(버퍼, 텍스처, 셰이더 등)을 생성 및 관리하는 역할을 한다.
	ComPtr<ID3D11Device> _device;
	// * DeviceContext : Device로 생성된 리소스들을 이용하여 실제 렌더링 작업을 수행하는 역할을 한다.
	ComPtr<ID3D11DeviceContext> _deviceContext;
	// * DeviceContext1 : 같은 컨텍스트의 D3D11.1 인터페이스 (_deviceContext.As() 로 얻는다. 별도 객체가 아니라 같은 객체의 다른 창구).
	//                    RenderBegin() 에서 "뷰포트 영역만" 지우는 ClearView 에만 쓴다.
	//                    11.1 런타임(Windows 8+)이 아니면 null 로 남고, 그때는 바 영역 없이 전체가 게임 배경색이 된다 (기능 저하일 뿐 크래시 아님).
	//                    _deviceContext 바로 아래 선언 -> 파괴 시 먼저 놓인다.
	ComPtr<ID3D11DeviceContext1> _deviceContext1;
	// * SwapChain  : 프론트 버퍼, 백버퍼 등 화면에 출력되는 버퍼에 대한 관리를 한다.(더블 버퍼링 등)
	ComPtr<IDXGISwapChain> _swapChain;

	// RTV
	/// * [RenderTargetView] : DirectX에서 렌더링 목적지, 즉 그래픽스 파이프라인 출력의 최종
	///                        목적지를 나타내는 객체이다.
	///                        그래픽스 데이터(픽셀)가 렌더링 되는 '화면' 또는 '표면'을 표현한다.
	ComPtr<ID3D11RenderTargetView> _renderTargetView;

	// 실제로 선택된 GPU 기능 레벨. 디바이스 생성 시 채워진다.
	D3D_FEATURE_LEVEL _featureLevel = D3D_FEATURE_LEVEL_11_0;

	// 현재 백버퍼 크기 (물리 픽셀). Resize 의 "같은 크기면 무시" 판단에 쓴다.
	uint _backBufferWidth = 0;
	uint _backBufferHeight = 0;

	std::vector<std::pair<uint, ResizeCallback>> _resizeListeners;
	std::vector<std::pair<uint, LogicalResolutionCallback>> _logicalResolutionListeners;
	uint _nextListenerId = 1;   // 두 리스너 목록이 id 를 공유한다 (서로 다른 목록이라 겹쳐도 상관없지만, 하나면 헷갈릴 일이 없다)

	// 뷰포트 = 게임 영역. SetViewport() 가 백버퍼 크기 + 논리 해상도로 계산한다.
	D3D11_Viewport _viewport = D3D11_Viewport::Undefined_viewport;

	// 게임 영역(뷰포트 안쪽) 배경색. 회색 — 바와 구분되어 레터박스 경계가 눈에 보인다.
	float _clearColor[4] = { 0.7f, 0.7f, 0.7f, 1.f };
	// 바(레터박스/필러박스) 영역 색. 검정 — 영화 레터박스처럼 "여긴 게임 화면이 아니다" 를 드러낸다.
	float _letterboxColor[4] = { 0.f, 0.f, 0.f, 1.f };
};

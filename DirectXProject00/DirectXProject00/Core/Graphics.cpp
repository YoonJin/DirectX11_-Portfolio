#include "pch.h"
#include "Graphics.h"

Graphics::Graphics()
{
}

Graphics::~Graphics()
{
	// 해제 순서는 Graphics.h 의 멤버 선언 순서(의 역순)가 보장한다. 여기서 직접 Release 하지 않는다.
#ifdef _DEBUG
	// [라이브 오브젝트 리포트]
	// 디바이스를 제외한 모든 객체를 먼저 놓은 뒤, 아직 살아있는 D3D 객체를 출력창에 보고한다.
	// 정상이라면 ID3D11Device 자신(Refcount 0, IntRef 는 남을 수 있음) 외에는 아무것도 나오지 않아야 한다.
	// 무언가 더 나온다면 어딘가에서 Release 가 빠진 것이다 — 리소스 관리자를 붙이기 전의 누수 검증 장치.
	ComPtr<ID3D11Debug> debug;
	if (_device && SUCCEEDED(_device.As(&debug)))
	{
		_renderTargetView.Reset();
		_swapChain.Reset();
		_deviceContext1.Reset();   // 같은 컨텍스트의 두 번째 참조. 둘 다 놓아야 Live ID3D11DeviceContext 가 남지 않는다.
		_deviceContext.Reset();
		debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
	}
#endif
}

// 렌더링을 어떻게 할지, 화면 렌더링 데이터를 결정해준다.
void Graphics::RenderBegin()
{
	// 화면을 출력하는데 사용할 RTV 데이터를 결정한다.
	// 주의 : &_renderTargetView 를 넘기면 ComPtr 이 기존 객체를 Release 해버린다. 반드시 GetAddressOf().
	_deviceContext->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), nullptr);

	// [2단계 클리어 — 바 영역 / 게임 영역]
	// 1. 백버퍼 전체를 바(레터박스) 색으로 지운다.
	//    ClearRenderTargetView 는 뷰포트를 무시하고 항상 RTV 전체를 지운다. 그래서 이것만 쓰면 바 영역도 게임 배경색이 되어
	//    레터박스가 보이지 않는다.
	_deviceContext->ClearRenderTargetView(_renderTargetView.Get(), _letterboxColor);

	// 2. 뷰포트(게임 영역)만 게임 배경색으로 다시 지운다.
	//    영역을 지정해서 지우려면 D3D11.1 의 ClearView 가 필요하다 (사각형 목록을 받아 그 안만 지운다).
	//    Windows 8+ / 런타임 11.1 — 이 프로젝트의 대상 환경에서는 항상 있다. 없으면(_deviceContext1 == null) 1 번만 적용되어
	//    전체가 바 색이 되므로, 그 경우에는 전체를 게임 배경색으로 지워 최소한 그림은 보이게 한다.
	//    대안인 "전체를 게임색으로 지우고 바 영역을 사각형 두 개로 그리기" 는 셰이더가 필요하므로 지금 단계에서는 ClearView 가 맞다.
	//    후속 작업에서 셰이더가 생기면 바 영역에 이미지를 그리는 식으로 확장할 수 있다.
	if (_deviceContext1)
	{
		// D3D11_RECT 는 정수 좌표다. Letterbox 는 남는 공간을 반으로 나누므로 .5 가 나올 수 있는데,
		// 여기서 버림(truncate)하면 뷰포트(래스터라이저가 float 그대로 씀)와 최대 1px 어긋날 수 있다. 바 색과 게임 배경색 경계의
		// 1px 차이는 눈에 띄지 않으므로 허용한다. (정확히 맞추고 싶으면 integerScale 옵션을 켜거나 Letterbox 에서 offset 을 floor 하면 된다)
		const D3D11_RECT gameRect = {
			static_cast<LONG>(_viewport.x),
			static_cast<LONG>(_viewport.y),
			static_cast<LONG>(_viewport.x + _viewport.width),
			static_cast<LONG>(_viewport.y + _viewport.height) };
		_deviceContext1->ClearView(_renderTargetView.Get(), _clearColor, &gameRect, 1);
	}
	else
	{
		_deviceContext->ClearRenderTargetView(_renderTargetView.Get(), _clearColor);
	}

	// 3. 설정한 RTV에 대응하는 viewports 정보를 설정한다. 이후 모든 드로우는 게임 영역 안으로만 들어간다.
	D3D11_VIEWPORT d3d11_viewport;
	d3d11_viewport.TopLeftX = _viewport.x;
	d3d11_viewport.TopLeftY = _viewport.y;
	d3d11_viewport.Width = _viewport.width;
	d3d11_viewport.Height = _viewport.height;
	d3d11_viewport.MinDepth = _viewport.min_depth;
	d3d11_viewport.MaxDepth = _viewport.max_depth;


	_deviceContext->RSSetViewports(1, &d3d11_viewport);
}


void Graphics::RenderEnd()
{
	/// <summary>
	/// * RenderBegin에서 적용한 백버퍼 내용을 프론트 버퍼와 서로 역할을 교환(swap)하는 기능이다.
	///
	///   스왑체인의 "Present" 메서드를 호출할 때 일어나는 것은 구성에 따라 다르다.
	///   주로 두가지 방식이 있다.
	///    1. 버퍼 교환(Swap) : 백 버퍼와 프론트 버퍼의 역할이 서로 바뀐다. 이 방식은 일반적으로 메모리를
	///                         효율적으로 사용하며, 복사 과정 없이 두 버퍼의 역할만 바꾸기 때문에 빠르다.
	///    2. 버퍼 복사(Blit or Bit-Block Transfer) : 백 버퍼에 있는 데이터가 프론트 버퍼로 복사된다.
	///                                            이 방식은 복사 과정이 필요하기 때문에 약간 느릴 수 있지만, 백버퍼의 내용이 유지된다.
	///
	///
	///    -> 교환 및 복사의 설정에 관련해서.
	///
	///       설정자체는 DXGI_SWAP_CHAIN_DESC 구조체에서 SwapEffect 필드를 사용하여 설정을 정의할 수 있다.
	///       여기에는 다양한 옵션들이 있으며, 각 옵션은 백버퍼와 프론트 버퍼 간의 다른 동작을 정의 한다.
	///
	///       다음은 몇가지 SwapEffect 옵션이다.
	///
	///       1. 'DXGI_SWAP_EFFECT_DISCARD'         : 이 옵션은 백버퍼의 내용을 프론트 버퍼로 복사한 후 백 버퍼의 내용을 폐기한다.
	///                                               즉, 백 버퍼의 내용이 더 이상 보장되지 않는다.
	///       2. 'DXGI_SWAP_EFFECT_SEQUENTIAL'      : 이 옵션은 백버퍼의 내용을 프론트 버퍼로 복사한 수 백 버퍼의 내용을 유지한다.
	///       3. 'DXGI_SWAP_EFFECT_FLIP_SWQUENTIAL' : 이 옵션은 버퍼를 "플립" 하는 동작을 수행한다. 즉, 프론트 버퍼와
	///                                               백 버퍼를 교환한다. 이 방식은 Windows 8 이후의 버전에서 사용이 가능하다.
	///       4. 'DXGI_SWAP_EFFECT_FLIP_DISCARD'    : 이 옵션도 버퍼를 "플립"하지만, 프레임을 플립한 후 백버퍼의 내용을 폐기한다.
	///                                               이 방식은 Windows10 이후의 버전에서 사용 가능하다.
	///
	///
	///    -> 플립 모델(flip model)이란?
	///
	///       프레임 버퍼링 방식 중 하나로, 그래픽스 어플리케이션에서 프레임을 렌더링 하고 표시하는 방법을 설명한다.
	///       플립 모델에서는 여러 버퍼(일반적으로 두 개 이상의 버퍼: 프론트 버퍼와 백 버퍼)를 사용하여 화면에 렌더링 될 프레임을 관리한다.
	///
	///       1. 프론트 버퍼(Front Buffer) : 화면에 현재 표시되고 있는 프레임의 데이터를 담고 있는 버퍼이다.
	///       2. 백 버퍼(Back Buffer)      : 화면에 표시되지 않는 비활성 버퍼에서 다음 프레임이 렌더링 되고 있는 버퍼이다.
	///                                      렌더링이 완료되면 프론트 버퍼와 교환(또는 복사) 된다.
	///
	///
	///       플립 모델에서는 다음과 같은 스텝을 거친다.
	///
	///       1. 백 버퍼에서 새 프레임을 렌더링 한다.
	///       2. 렌더링이 완료되면, 백 버퍼와 프론트 버퍼를 "플립"한다. 즉, 백 버퍼가 프론트 버퍼가 되고,
	///          프론트 버퍼가 백 버퍼가 된다.
	///       3. 화면은 업데이트 되며 새로운 프레임이 표시된다.
	/// </summary>

	/// <summary>
	/// * 첫 번째 인자 '1'은 VSync를 의미한다. 이 값이 1이면, 스왑체인은 VSync가 일어날 때까지
	/// 기다린다. (즉 모니터의 "리프레시 레이트(주사율)"에 동기화). 값이 0이면 VSync를 기다리지
	/// 않고 즉시 스왑이 이루어진다.
	/// * 두 번째 인자 '0'은 스왑 체인 옵션을 나타낸다.
	///   현재 0이므로 추가 옵션 없이 기본 옵션을 사용한다.
	///   (여러 호출 방식에 대한 옵션을 설정할 수 있는 비트 플래그)
	/// </summary>
	HRESULT hr = _swapChain->Present(1, 0);
	CHECK(hr);
}


void Graphics::CreateDeviceAndSwapChain()
{
	// 스왑체인 관련 설정
	DXGI_SWAP_CHAIN_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	{
		// 버퍼 화면 크기
		desc.BufferDesc.Width = 0;
		desc.BufferDesc.Height = 0;
		// 화면 주사율
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		// 각 색상 채널은 8비트씩 사용하며 총 0 ~ 255만큼의 색을 표현할 수 있다.
		// UNORM은 UnsignedNormalized를 나타내며, 0 ~ 255까지의 값이 0 ~ 1까지의 비율로 정규화
		// 되었다는 것을 의미한다.
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		// 사용할 버퍼 갯수
		desc.BufferCount = 1;
		// 백버퍼로 사용을 하겠다.
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		// 윈도우 핸들 전달
		desc.OutputWindow = Settings::Get().GetWindowHandle();
		desc.Windowed = TRUE;
		// 기본 Effect로 설정하겠다. (설정 안하겠다.)
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		// 사용할 레벨 목록
		// 목록에 기재되어 있는 레벨중 GPU가 사용할 수 있는
		// 가장 최고 레벨을 우선적으로 사용하도록 한다.
		// GPU 하드웨어의 기능 레벨
		std::vector<D3D_FEATURE_LEVEL> feature_levels
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};

		// 디바이스 생성 플래그
		UINT flags = 0;
#ifdef _DEBUG
		// [D3D11 디버그 레이어]
		// 잘못된 API 사용, 바인딩 오류, 리소스 누수를 Visual Studio 출력창에 보고한다.
		// Windows "그래픽 도구" 선택적 기능이 설치되어 있어야 한다. 없으면 디바이스 생성이 실패하므로
		// 실패 시 플래그를 빼고 한 번 더 시도한다.
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		// 람다로 묶은 이유 : 디버그 레이어 실패 시 같은 인자로 재시도하기 위해서.
		auto create = [&](UINT createFlags) -> HRESULT
		{
			return ::D3D11CreateDeviceAndSwapChain
			(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE, //  DirectX가 GPU 하드웨어 가속을 사용하여 그래픽스 작업을 수행하도록 지시한다.
				nullptr,
				createFlags,
				feature_levels.data(),                        // GPU 하드웨어의 기능 레벨
				static_cast<UINT>(feature_levels.size()),     // 목록 길이. (이전에는 1 을 넘겨 11_1 만 시도하는 버그가 있었다)
				D3D11_SDK_VERSION,                            // 어떤 버전의 DirectX API를 사용할지 결정
				&desc,
				_swapChain.GetAddressOf(),
				_device.GetAddressOf(),
				&_featureLevel,                               // 실제로 선택된 기능 레벨을 받아둔다
				_deviceContext.GetAddressOf()
			);
		};

		HRESULT hr = create(flags);
#ifdef _DEBUG
		if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG))
		{
			flags &= ~D3D11_CREATE_DEVICE_DEBUG;
			hr = create(flags);
		}
#endif
		CHECK(hr);
	}

	// [Alt+Enter 가로채기 차단]
	// DXGI 는 기본적으로 Alt+Enter 를 가로채 독점 전체화면(exclusive fullscreen) 으로 전환한다.
	// 이 프레임워크는 전체화면을 직접 제어할 예정이고, Blt 모델에서 독점 전체화면 전환은
	// 처리하지 않은 경로(백버퍼 크기 불일치 등)가 많아 크래시 원인이 되므로 끈다.
	// 스왑체인을 만든 팩토리는 IDXGISwapChain::GetParent 로 바로 얻을 수 있다.
	{
		ComPtr<IDXGIFactory> factory;
		HRESULT hr = _swapChain->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
		CHECK(hr);

		// DXGI_MWA_NO_ALT_ENTER : Alt+Enter 전체화면 토글을 DXGI 가 처리하지 않게 한다.
		hr = factory->MakeWindowAssociation(Settings::Get().GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER);
		CHECK(hr);
	}

	// [D3D11.1 컨텍스트 인터페이스]
	// RenderBegin() 의 ClearView(영역 지정 클리어)용. As() 는 QueryInterface 라 새 객체를 만드는 게 아니라 같은 컨텍스트에
	// 참조 하나를 더 얻는 것이다. 11.1 런타임이 아니면 실패하는데, 그때는 폴백 경로(전체 클리어)가 있으므로 CHECK 하지 않고 결과를 무시한다.
	// 실패 시 ComPtr 은 null 로 남는다.
	_deviceContext.As(&_deviceContext1);
}

void Graphics::CreateRenderTargetView(const uint& width, const uint& height)
{
	// 최초 생성 경로.
	// 스왑체인을 만들 때 BufferDesc.Width/Height 를 0 으로 넘겼으므로, 여기서 실제 클라이언트 크기로 한 번 맞춰준다.
	// 기존의 버퍼 내용에서 수정할 부분의 데이터만 기재해준다.
	// 인자로 0과 DXGI_FORMAT_UNKNOWN 이라고 기재한 부분은, 이전에 설정한 데이터의 포맷을 유지하게 된다.
	HRESULT hr = _swapChain->ResizeBuffers(
		0,
		width,
		height,
		DXGI_FORMAT_UNKNOWN,
		0);

	CHECK(hr);

	CreateBackBufferResources(width, height);
}

bool Graphics::Resize(const uint& width, const uint& height)
{
	// 0 크기(최소화)면 아무것도 하지 않는다. (실패가 아니므로 true)
	if (width == 0 || height == 0) return true;
	// 같은 크기 — 백버퍼를 다시 만들 이유가 없다. (창 이동만 한 뒤의 WM_EXITSIZEMOVE, 생성 직후 첫 WM_SIZE 가 여기로 온다)
	// 논리 해상도 변경은 이제 SetLogicalResolution() 이 따로 처리하므로 여기서 뷰포트를 다시 볼 필요가 없다.
	if (width == _backBufferWidth && height == _backBufferHeight) return true;
	if (!_swapChain) return false;

	// [순서가 중요하다]
	// ResizeBuffers 는 백버퍼를 참조하는 객체가 하나라도 살아있으면 DXGI_ERROR_INVALID_CALL 로 실패한다.
	//   1. 파이프라인에서 RTV 바인딩을 푼다 (컨텍스트가 참조를 들고 있다)
	//   2. RTV 를 놓는다
	//   3. ResizeBuffers
	//   4. 새 백버퍼로 RTV 재생성
	//   5. 뷰포트 갱신
	//   6. 구독자에게 통지
	// 주의 : 구독자 중 누군가가 백버퍼 자체에 대한 뷰(SRV 등)를 들고 있다면 여기서 ResizeBuffers 가 실패한다.
	//        지금은 그런 구독자가 없다. 생기면 "리사이즈 전" 통지 단계를 추가해서 먼저 놓게 해야 한다.
	_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	_renderTargetView.Reset();

	HRESULT hr = _swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	CHECK(hr);
	// CHECK 는 Release 빌드에서 사라진다. 실패한 채로 진행하면 RTV 가 없는 상태로 매 프레임 그리게 되므로 여기서 멈춘다.
	// (크기 필드를 갱신하지 않았으므로 다음 리사이즈 요청에서 다시 시도된다. false 를 돌려 호출자가 요청을 되살릴 수 있게 한다)
	if (FAILED(hr)) return false;

	if (!CreateBackBufferResources(width, height)) return false;
	SetViewport(width, height);

	// 콜백 안에서 Add/RemoveResizeListener 가 호출되면 벡터가 재할당/이동되어 순회 중인 반복자가 무효화된다.
	// (예: 리사이즈 시 자신을 재생성하고 다시 구독하는 리소스) 복사본을 순회해서 이를 막는다.
	const auto listeners = _resizeListeners;
	for (const auto& [id, callback] : listeners)
		callback(width, height);

	return true;
}

bool Graphics::CreateBackBufferResources(const uint& width, const uint& height)
{
	// 백버퍼를 가져와 렌더 타겟 뷰를 생성하는 부분
/*
   백버퍼를 "ID3D11Texture2D"로 받아 사용하는 이유는,
   스왑 체인의 버퍼는 사실상 2D 텍스처이기 때문이다.

   하지만, _swapChain->GetBuffer 함수는 "ID3D11Resource" 인터페이스를 상속받은 여러 유형의 객체로 버퍼에 접근할 수 있게한다.

   * 아래는 ID3DResource를 상속받은 객체의 유형들 이다.

   1. ID3D11Buffer
   2. ID3D11Texture1D
   3. ID3D11Texture2D
   4. ID3D11Texture3D

   * 아래는 _swapChain->GetBuffer()의 인자 종류이다.

   1. 받아올 버퍼의 데이터를 특정하는 인덱스
	  ->지금 현재는 백버퍼를 1개만 사용한다고 설정했으므로 인덱스는 0이다.
   2. BackBuffer를 어떤 인터페이스 자료형으로 반환해 줄지 결정할 수 있도록 __uuidof를 사용하여 특정 컴인터페이스의 GUID를 넣어준다.

   3. ID3D11Texture2D* 로 되어있는 변수에 데이터를 넣기 위해 void**로 형변환을 한 후 대입해준다.
	   -> 포인터끼리의 변환은 reinterpret_cast를 주로 사용한다.

   * IID_PPV_ARGS(p) 매크로는 위 2, 3 번 인자 쌍 ( __uuidof(*p), reinterpret_cast<void**>(p) ) 을 한 번에 만들어준다.
	 타입과 GUID 가 어긋나는 실수를 컴파일 타임에 막아주므로 이쪽을 쓴다.
*/
	ComPtr<ID3D11Texture2D> backBuffer;
	HRESULT hr = _swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
	CHECK(hr);
	if (FAILED(hr)) return false;

	hr = _device->CreateRenderTargetView(backBuffer.Get(), nullptr, _renderTargetView.GetAddressOf());
	CHECK(hr);
	if (FAILED(hr)) return false;

	// 크기 필드는 두 호출이 모두 성공한 뒤에만 갱신한다.
	// 실패했는데 갱신해버리면 Resize() 의 "같은 크기면 무시" 조건에 걸려 영원히 복구되지 않는다.
	_backBufferWidth = width;
	_backBufferHeight = height;
	return true;

	// backBuffer 는 ComPtr 이므로 스코프를 벗어나며 자동으로 Release 된다.
	// (RTV 가 내부적으로 백버퍼 참조를 들고 있으므로 여기서 놓아도 백버퍼는 살아있다)
}

void Graphics::SetViewport(const uint& width, const uint& height)
{
	// width/height 는 백버퍼(물리) 크기. 뷰포트는 논리 해상도의 종횡비를 유지하도록 그 안에 맞춘다 (레터박스/필러박스).
	// 이전에는 (0, 0, width, height) 로 백버퍼 전체였다 — 그 방식은 창 종횡비가 바뀌면 그림이 늘어난다.
	//
	// [좌표계 흐름]
	//   논리 좌표 (0..논리W, 0..논리H)
	//      | 직교 투영 행렬 (논리 해상도로 한 번만 생성 — 후속 SpriteBatch 의 책임)
	//      v
	//   NDC (-1..1)
	//      | 뷰포트 변환 (리사이즈마다 여기서 갱신 — 이 함수의 책임)
	//      v
	//   백버퍼 물리 픽셀 (레터박스 안쪽)
	// 투영은 고정이고 뷰포트만 움직이므로, 그리는 쪽은 리사이즈를 전혀 몰라도 된다.
	const uint logicalWidth = Settings::Get().GetLogicalWidth();
	const uint logicalHeight = Settings::Get().GetLogicalHeight();

	_viewport = D3D11_Viewport::Letterbox(width, height, logicalWidth, logicalHeight);
}

void Graphics::SetLogicalResolution(uint width, uint height)
{
	if (width == 0 || height == 0) return;
	if (width == Settings::Get().GetLogicalWidth() && height == Settings::Get().GetLogicalHeight()) return;

	// 순서 : Settings 갱신 → 뷰포트 재계산 → 통지.
	// 구독자(SpriteBatch::BuildProjection)는 콜백 인자보다 Settings 를 읽는 편이 자연스러우므로 Settings 가 먼저 맞아 있어야 한다.
	// 백버퍼 크기는 그대로다 — 리사이즈 리스너는 부르지 않는다.
	Settings::Get().SetLogicalResolution(width, height);
	SetViewport(_backBufferWidth, _backBufferHeight);

	// Resize() 와 같은 이유로 복사본을 순회한다 (콜백 안에서 Add/Remove 가 불려도 반복자가 무효화되지 않게).
	const auto listeners = _logicalResolutionListeners;
	for (const auto& [id, callback] : listeners)
		callback(width, height);
}

uint Graphics::AddLogicalResolutionListener(LogicalResolutionCallback callback)
{
	const uint id = _nextListenerId++;
	_logicalResolutionListeners.emplace_back(id, std::move(callback));
	return id;
}

void Graphics::RemoveLogicalResolutionListener(uint id)
{
	for (auto it = _logicalResolutionListeners.begin(); it != _logicalResolutionListeners.end(); ++it)
	{
		if (it->first == id)
		{
			_logicalResolutionListeners.erase(it);
			return;
		}
	}
}

uint Graphics::AddResizeListener(ResizeCallback callback)
{
	const uint id = _nextListenerId++;
	_resizeListeners.emplace_back(id, std::move(callback));
	return id;
}

void Graphics::RemoveResizeListener(uint id)
{
	for (auto it = _resizeListeners.begin(); it != _resizeListeners.end(); ++it)
	{
		if (it->first == id)
		{
			_resizeListeners.erase(it);
			return;
		}
	}
}

#include "pch.h"
#include "Shader.h"

namespace
{
	// 파일 소스를 지정한 엔트리/프로파일로 컴파일한다. 결과는 바이트코드 블롭, 실패 시 null.
	// 1.5단계(TestPattern)·5단계(SpriteBatch)에 두 벌 있던 D3DCompile 코드를 한 곳으로 모은 것이다.
	// D3DCompileFromFile 을 쓰는 이유 : 파일을 직접 읽어 D3DCompile 에 넘기는 것보다 단순하고 #include 도 기본 핸들러가 처리한다.
	// (HLSL 파일의 주석은 영문이어야 한다 — 컴파일러가 소스를 ANSI 로 읽으므로 UTF-8 한글이 들어가면 X3000 경고가 날 수 있다)
	ComPtr<ID3DBlob> CompileFromFile(const std::wstring& path, const char* entry, const char* target)
	{
		UINT flags = 0;
#ifdef _DEBUG
		// 디버그 정보 포함 + 최적화 끔 : PIX/RenderDoc 에서 소스 레벨로 볼 수 있다.
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		ComPtr<ID3DBlob> code, errors;
		HRESULT hr = ::D3DCompileFromFile(
			path.c_str(),
			nullptr,                       // 매크로
			D3D_COMPILE_STANDARD_FILE_INCLUDE,   // #include 를 파일 기준 상대 경로로 처리하는 기본 핸들러
			entry, target,
			flags, 0,
			code.GetAddressOf(), errors.GetAddressOf());

		// [실패 원인을 구분해서 찍는다]
		//   파일 없음 : errors 블롭이 비어 있고 hr 이 파일 시스템 에러(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) 등)다. 경로를 찍는다.
		//   컴파일 실패 : errors 블롭에 "<path>(line,col): error X...: ..." 형식의 메시지가 온다. CHECK 가 멈추기 전에 먼저 찍어야 원인을 볼 수 있다.
		if (errors)
			::OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
		else if (FAILED(hr))
			::OutputDebugStringW((L"[Shader] cannot open shader file: " + path + L"\n").c_str());
		CHECK(hr);
		return code;
	}
}

bool Shader::Compile(Graphics* graphics, const std::wstring& path,
                     const char* vsEntry, const char* psEntry,
                     const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
	if (!graphics) return false;
	ID3D11Device* device = graphics->GetDevice();

	ComPtr<ID3DBlob> vs = CompileFromFile(path, vsEntry, "vs_4_0");
	ComPtr<ID3DBlob> ps = CompileFromFile(path, psEntry, "ps_4_0");
	// Release 빌드에서는 CHECK 가 사라진다. 컴파일 실패 시 null 블롭을 역참조하지 않도록 여기서 멈춘다.
	if (!vs || !ps) return false;

	HRESULT hr = device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, _vs.ReleaseAndGetAddressOf());
	CHECK(hr);
	if (FAILED(hr)) return false;
	hr = device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, _ps.ReleaseAndGetAddressOf());
	CHECK(hr);
	if (FAILED(hr))
	{
		_vs.Reset();
		return false;
	}

	// [입력 레이아웃]
	// 정점 버퍼의 바이트 배치와 VS 의 입력 시그니처(VSIn)를 이어주는 객체. 생성 시 VS 바이트코드가 필요한 이유 :
	// 런타임이 시맨틱 이름/형식이 셰이더 입력과 맞는지 검증하기 때문이다. desc 가 비어 있으면 정점 입력이 없는 셰이더다.
	_layout.Reset();
	if (!layout.empty())
	{
		hr = device->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()),
		                               vs->GetBufferPointer(), vs->GetBufferSize(), _layout.GetAddressOf());
		CHECK(hr);
		if (FAILED(hr))
		{
			_vs.Reset();
			_ps.Reset();
			return false;
		}
	}
	_layoutHash = HashLayout(layout);
	return true;
}

size_t Shader::HashLayout(const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
	// FNV-1a. 암호학적 강도는 필요 없다 — 같은 레이아웃이면 같은 값, 다른 레이아웃이면 거의 확실히 다른 값이면 된다.
	size_t h = 14695981039346656037ull;
	auto mix = [&h](size_t v) { h ^= v; h *= 1099511628211ull; };
	for (const D3D11_INPUT_ELEMENT_DESC& e : layout)
	{
		for (const char* p = e.SemanticName; p && *p; ++p) mix(static_cast<unsigned char>(*p));
		mix(e.SemanticIndex); mix(e.Format); mix(e.InputSlot); mix(e.AlignedByteOffset); mix(e.InputSlotClass); mix(e.InstanceDataStepRate);
	}
	mix(layout.size());
	return h;
}

void Shader::Bind(ID3D11DeviceContext* context) const
{
	// 레이아웃이 없으면 null 을 명시적으로 건다 — 직전 드로우가 남긴 레이아웃을 물려받지 않는다 (7단계 규칙 : 자기 상태는 자기가 명시).
	context->IASetInputLayout(_layout.Get());
	context->VSSetShader(_vs.Get(), nullptr, 0);
	context->PSSetShader(_ps.Get(), nullptr, 0);
}

#pragma once

// VS + PS + 입력 레이아웃 한 묶음. (6단계)
//
// [왜 VS/PS 를 한 객체로 묶는가]
//   이 프로젝트의 셰이더는 항상 VS/PS 쌍으로 쓰인다 (TestPattern, Sprite 모두 파일 하나에 VS/PS 엔트리가 같이 있다).
//   따로 관리하면 "이 VS 에 어느 PS 를 짝지었더라" 를 사용자가 기억해야 한다. 묶으면 Bind() 한 번으로 끝난다.
//
// [입력 레이아웃도 여기 있는 이유]
//   입력 레이아웃은 정점 버퍼의 바이트 배치와 VS 의 입력 시그니처를 이어주는 객체라 생성 시 VS 바이트코드가 필요하다.
//   바이트코드를 갖고 있는 순간(컴파일 직후)에 같이 만드는 것이 가장 단순하다. 레이아웃 desc 가 비어 있으면 만들지 않는다
//   (SV_VertexID 만 쓰는 TestPattern). 같은 배치를 쓰는 다른 VS 에 레이아웃을 재사용할 수도 있지만, 지금은 그런 경우가 없다.
//
// [런타임 컴파일 + 4_0 프로파일]
//   D3DCompileFromFile 로 실행 시점에 컴파일한다. 오프라인(.cso) 으로 바꾸더라도 CreateXxxShader 에 넘기는 것은 같은 바이트코드다.
//   프로파일은 vs_4_0 / ps_4_0 고정 — 이 프레임워크가 허용하는 최저 기능 레벨(10_0)에서도 돌아야 한다 (1.5단계 결정).
//   5_0 을 쓰면 11_0 미만 디바이스에서 CreateVertexShader 가 실패한다.
class Shader final
{
public:
	Shader() = default;
	~Shader() = default;
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	// path 는 절대 경로 (ShaderManager 가 정규화해서 넘긴다). 파일을 못 찾은 경우와 컴파일 실패를 출력창 메시지로 구분한다.
	// 실패 시 false 이고 멤버는 전부 null — Bind() 는 null 셰이더를 바인딩하고 아무것도 그리지 않을 뿐 죽지는 않는다.
	bool Compile(Graphics* graphics, const std::wstring& path,
	             const char* vsEntry, const char* psEntry,
	             const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout);

	// VS, PS, 입력 레이아웃을 바인딩한다. 레이아웃이 없는 셰이더는 null 을 바인딩한다 (정점 입력 없음을 명시).
	void Bind(ID3D11DeviceContext* context) const;

	// 입력 레이아웃 desc 의 지문 (시맨틱 이름/인덱스/포맷/슬롯/오프셋). ShaderManager 가 캐시 적중 시 "같은 파일·엔트리를 다른 레이아웃으로
	// 요청했다" 를 Debug 에서 잡는 데 쓴다 — 레이아웃은 키에 없으므로(ShaderManager.h), 이 검사가 없으면 엉뚱한 레이아웃이 조용히 바인딩되고
	// 드로우 시점의 디버그 레이어 에러로만 드러난다.
	static size_t HashLayout(const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout);
	size_t GetLayoutHash() const { return _layoutHash; }

private:
	size_t _layoutHash = 0;
	ComPtr<ID3D11VertexShader> _vs;
	ComPtr<ID3D11PixelShader>  _ps;
	ComPtr<ID3D11InputLayout>  _layout;
};

#include "pch.h"
#include "ShaderManager.h"
#include "Path.h"

ShaderManager::ShaderManager(Graphics* graphics)
	: _graphics(graphics)
{
}

ShaderManager::~ShaderManager()
{
	// unique_ptr 가 Shader 를 지우면 그 안의 ComPtr 이 VS/PS/레이아웃을 Release 한다. 전부 Graphics 보다 먼저다.
}

Shader* ShaderManager::Load(const std::wstring& relativePath, const char* vsEntry, const char* psEntry,
                            const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout)
{
	// 키 = 정규화된 소문자 절대 경로 + "|" + VS 엔트리 + "|" + PS 엔트리 (엔트리는 ASCII 식별자라 wchar 로 그대로 넓힌다).
	// 컴파일에는 원래 대소문자의 경로를 넘긴다 — 에러 메시지에 이 절대 경로와 줄 번호가 찍힌다.
	const std::wstring path = Path::Normalize(relativePath);
	std::wstring key = Path::MakeKey(path);
	key += L'|';
	for (const char* p = vsEntry; p && *p; ++p) key += static_cast<wchar_t>(*p);
	key += L'|';
	for (const char* p = psEntry; p && *p; ++p) key += static_cast<wchar_t>(*p);

	auto found = _shaders.find(key);
	if (found != _shaders.end())
	{
		// 레이아웃은 키에 없다 (ShaderManager.h). 같은 파일·엔트리를 다른 레이아웃으로 요청하면 첫 사용자의 레이아웃이 조용히 돌아가므로 Debug 에서 잡는다.
		assert(found->second->GetLayoutHash() == Shader::HashLayout(layout)
		       && "ShaderManager: same shader requested with a different input layout — add the layout to the cache key");
		return found->second.get();
	}

	auto shader = std::make_unique<Shader>();
	if (!shader->Compile(_graphics, path, vsEntry, psEntry, layout))
		return nullptr;   // 원인은 Shader::Compile 이 이미 출력창에 찍었다. 실패한 키는 캐시에 남기지 않는다

	Shader* raw = shader.get();
	_shaders.emplace(key, std::move(shader));
	return raw;
}

#include "pch.h"
#include "TextureAtlas.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Path.h"
#include <fstream>
#include <sstream>

namespace
{
	// 정의 파일은 ASCII 라 narrow 로 읽는다. 파일 경로만 wide (ifstream 은 MSVC 에서 wchar_t* 경로를 받는다).
	void Warn(const std::wstring& path, int line, const char* what)   // what 에 "skipped" 여부를 포함한다 — 모든 경고가 건너뛰기는 아니다
	{
		// 출력용으로만 좁힌다. 경로에 비 ASCII 가 있으면 '?' 로 나오지만 메시지일 뿐이다. (VS 출력창의 "파일(줄): 메시지" 형식 — 더블클릭하면 열린다)
		std::string narrowPath;
		narrowPath.reserve(path.size());
		for (wchar_t c : path) narrowPath += (c < 0x80) ? static_cast<char>(c) : '?';
		::OutputDebugStringA(("[TextureAtlas] " + narrowPath + "(" + std::to_string(line) + "): " + what + "\n").c_str());
	}
}

bool TextureAtlas::LoadFromFile(TextureManager* textures, const std::wstring& relativePath)
{
	assert(textures && "TextureAtlas::LoadFromFile: TextureManager is null");
	if (!textures) return false;
	const std::wstring path = Path::Normalize(relativePath);
	std::ifstream file(path.c_str());
	if (!file)
	{
		::OutputDebugStringW((L"[TextureAtlas] cannot open atlas definition: " + path + L"\n").c_str());
		return false;
	}

	// 로컬에 파싱하고 성공했을 때만 멤버로 옮긴다 — 실패한 재로드가 살아 있는 AtlasFrame* 를 건드리지 않게 (헤더의 "다시 부르면").
	Texture* texture = nullptr;
	std::map<std::string, AtlasFrame> frames;

	std::string line;
	int lineNumber = 0;
	while (std::getline(file, line))
	{
		++lineNumber;
		if (!line.empty() && line.back() == '\r') line.pop_back();   // CRLF 파일을 text 모드로 읽어도 '\r' 이 남는 경우(정의 파일이 LF 로 저장된 경우 대비) 방어

		std::istringstream tokens(line);
		std::string keyword;
		if (!(tokens >> keyword) || keyword[0] == '#') continue;   // 빈 줄 / 주석

		if (keyword == "texture")
		{
			// 이 줄의 나머지 전체가 경로 (공백 포함 가능). 경로는 wide 로 올려 TextureManager 에 넘긴다 — ASCII 경로만 가정한다.
			std::string rest;
			std::getline(tokens, rest);
			const size_t begin = rest.find_first_not_of(" \t");
			if (begin == std::string::npos) { Warn(path, lineNumber, "texture: path missing - line skipped"); continue; }
			const size_t end = rest.find_last_not_of(" \t");
			rest = rest.substr(begin, end - begin + 1);   // 양끝 공백 제거 — 뒤에 붙은 공백이 파일명에 들어가면 원인 찾기 어려운 로드 실패가 된다
			if (texture) Warn(path, lineNumber, "texture: repeated 'texture' line - the previous texture is kept unless this one loads");
			const std::wstring texturePath(rest.begin(), rest.end());
			if (Texture* loaded = textures->Load(texturePath)) texture = loaded;
			else Warn(path, lineNumber, "texture: load failed (see the TextureManager message above) - line skipped");
			continue;
		}

		if (keyword == "frame")
		{
			std::string name;
			float x, y, w, h;
			if (!(tokens >> name >> x >> y >> w >> h)) { Warn(path, lineNumber, "frame: expected <name> <x> <y> <w> <h> - line skipped"); continue; }
			if (!(w > 0.f && h > 0.f) || !(x >= 0.f && y >= 0.f)) { Warn(path, lineNumber, "frame: x/y must be >= 0 and w/h > 0 - line skipped"); continue; }   // !(a>b) 꼴이라 NaN 도 걸린다
			AtlasFrame frame;
			frame.source = { x, y, w, h };
			float px, py;
			if (tokens >> px)   // 피벗은 선택 항목. 없으면 (0, 0) = 좌상단. 하나만 있으면 오타일 가능성이 높으므로 조용히 (0,0) 으로 가지 않는다
			{
				if (tokens >> py) frame.pivot = { px, py };
				else { Warn(path, lineNumber, "frame: pivot needs both <pivotX> <pivotY> - line skipped"); continue; }
			}
			if (frames.count(name)) Warn(path, lineNumber, "frame: duplicate name - overwritten with this line");
			frames[name] = frame;
			continue;
		}

		Warn(path, lineNumber, "unknown keyword - line skipped");
	}

	if (!texture)
	{
		::OutputDebugStringW((L"[TextureAtlas] no usable 'texture' line in " + path + L" - atlas unchanged\n").c_str());
		return false;
	}
	_texture = texture;
	_frames = std::move(frames);
	return true;
}

const AtlasFrame* TextureAtlas::GetFrame(const std::string& name) const
{
	auto found = _frames.find(name);
	return found == _frames.end() ? nullptr : &found->second;
}

std::vector<const AtlasFrame*> TextureAtlas::GetFrames(const std::string& prefix) const
{
	// map 은 키 순으로 정렬되어 있으므로 접두사로 시작하는 키들은 lower_bound 부터 연속이다.
	std::vector<const AtlasFrame*> result;
	for (auto it = _frames.lower_bound(prefix); it != _frames.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
		result.push_back(&it->second);
	return result;
}

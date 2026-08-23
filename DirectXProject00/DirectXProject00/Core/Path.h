#pragma once
#include "pch.h"

// 에셋 경로 헬퍼.
//
// [왜 "작업 디렉터리 상대 경로" 로 파일을 열면 안 되는가]
//   VS 디버거(F5)로 실행하면 작업 디렉터리가 프로젝트 폴더(DirectXProject00/)이고,
//   탐색기에서 exe 를 더블클릭하면 x64/Debug/ 가 된다. "image/0.png" 같은 상대 경로는 둘 중 한 환경에서 반드시 깨진다.
//   그래서 exe 의 실제 위치(GetModuleFileNameW)를 기준으로 리포 루트를 찾고, 모든 에셋은 "리포 루트 상대" 로 연다.
//   (빌드 이벤트로 에셋을 출력 폴더에 복사하는 방식은 6단계 리소스 관리자에서 검토한다)
namespace Path
{
	// exe 가 있는 폴더. 실패하면 빈 경로.
	inline std::filesystem::path ExeDirectory()
	{
		wchar_t buffer[MAX_PATH] = {};
		const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		if (length == 0 || length >= MAX_PATH) return {};
		return std::filesystem::path(buffer).parent_path();
	}

	// 리포 루트 = "image" 폴더를 가진 가장 가까운 상위 디렉터리.
	// 이 프로젝트는 exe 가 <root>/x64/Debug/ 에 있으므로 보통 두 단계 위에서 찾는다.
	// 못 찾으면 exe 폴더를 돌려주고 출력창에 경고한다 — 이후 파일 열기가 실패해 로더 쪽 assert 가 절대 경로를 찍어준다.
	inline std::filesystem::path AssetRoot()
	{
		static const std::filesystem::path cached = []() -> std::filesystem::path
		{
			const std::filesystem::path exeDir = ExeDirectory();
			std::error_code ec;
			for (std::filesystem::path dir = exeDir; !dir.empty(); dir = dir.parent_path())
			{
				if (std::filesystem::is_directory(dir / L"image", ec)) return dir;
				if (dir == dir.root_path()) break;   // 드라이브 루트까지 왔다 — parent_path() 가 자기 자신을 돌려주므로 여기서 끊는다
			}
			::OutputDebugStringW((L"[Path] AssetRoot: 'image' folder not found above " + exeDir.wstring() + L" — falling back to exe directory\n").c_str());
			return exeDir;
		}();
		return cached;
	}

	// 리포 루트 상대 경로 → 정규화된 절대 경로 (6단계. TextureManager / ShaderManager 가 공유한다).
	// weakly_canonical : ".." / "." 제거, 구분자 통일, 존재하는 부분은 심볼릭 링크 해소. 존재하지 않는 파일에도 동작하므로
	// 잘못된 경로도 정규화는 되고, 이후 열기 단계에서 실패한다 (관리자가 nullptr 를 돌려주는 계약과 맞는다).
	// error_code 판을 쓴다 — 잘못된 경로로 예외가 나는 것보다 "정규화는 되되 로드가 실패" 하는 쪽이 이 프레임워크의 에러 규칙이다.
	inline std::wstring Normalize(const std::wstring& relativePath)
	{
		std::error_code ec;
		const std::filesystem::path full = AssetRoot() / relativePath;
		std::filesystem::path canonical = std::filesystem::weakly_canonical(full, ec);
		if (ec) canonical = full.lexically_normal();
		return canonical.wstring();
	}

	// 캐시 키 : 정규화된 경로의 소문자판. Windows 파일 시스템은 대소문자를 구분하지 않으므로
	// L"image/0.png" 와 L"IMAGE\0.PNG" 가 같은 키가 되어야 한다. 파일을 열 때는 Normalize() 의 원래 대소문자 경로를 쓴다.
	inline std::wstring MakeKey(const std::wstring& normalizedPath)
	{
		// std::towlower 가 아니라 CharLowerBuffW 를 쓴다 — 전자는 기본 "C" 로케일에서 ASCII 만 접고, 후자는 NTFS 가 파일명 비교에 쓰는
		// 것과 같은 Win32 대소문자 표(비 ASCII 문자 포함)를 쓴다. 둘이 다르면 같은 파일이 두 번 로드될 수 있다.
		std::wstring key = normalizedPath;
		if (!key.empty()) ::CharLowerBuffW(key.data(), static_cast<DWORD>(key.size()));
		return key;
	}
}

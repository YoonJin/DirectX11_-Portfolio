#pragma once
#include <map>
#include "SpriteBatch.h"   // RECT_F

class Texture;
class TextureManager;

// 아틀라스의 프레임 하나 = 시트 안의 텍셀 사각형 + 피벗.
//
// [텍셀 좌표를 저장하고 UV 는 그릴 때 계산한다]
//   아틀라스 정의는 사람이 읽고 편집해야 하므로 픽셀 단위 정수가 자연스럽다. UV(0..1) 는 텍스처 크기에 종속이라 시트를 다시 패킹하면 틀어진다.
//   변환은 SpriteBatch::Draw() 가 5단계부터 이미 하고 있다 (src 인자).
//
// [피벗을 애니메이션이 아니라 프레임 데이터에 둔다]
//   DNF 류 캐릭터 스프라이트는 프레임마다 크기가 다르고 발 위치(피벗)가 정해져 있다. 그리는 쪽은 "피벗이 놓일 논리 좌표" 만 주고,
//   dst.x - pivot 은 SpriteBatch 의 오버로드가 한다. 회전은 9단계.
struct AtlasFrame
{
	RECT_F   source = { 0.f, 0.f, 0.f, 0.f };   // 텍셀 좌표 (x, y, w, h)
	XMFLOAT2 pivot = { 0.f, 0.f };               // 프레임 내 기준점 (텍셀, 좌상단 기준). 캐릭터는 보통 발 중앙
};

// 텍스처 한 장 + 이름 → 프레임 사각형. (8단계)
//
// [아틀라스는 텍스처가 아니라 "텍스처 + 사각형 목록" 이라는 데이터다]
//   6단계의 Texture 를 상속하거나 감싸지 않고 Texture* 를 참조하는 별개 클래스로 둔다. 같은 텍스처를 여러 아틀라스 정의가 가리킬 수도 있고
//   (캐릭터/이펙트가 한 시트에), 아틀라스가 없는 단일 텍스처도 계속 그릴 수 있어야 한다. 텍스처는 TextureManager 가 소유한다.
//   그래서 아틀라스는 SpriteBatch 를 한 줄도 바꾸지 않는다 — Draw() 의 src 인자가 5단계부터 있었기 때문이다.
//
// [정의 파일은 가장 단순한 텍스트]
//   JSON 파서를 붙이면 외부 의존이 생기고 이 프로젝트 규칙(Windows SDK 외 의존 없음)에 어긋난다. "이름 x y w h" 한 줄 한 프레임이면 ifstream 으로 충분하다.
//   실제 프로덕션은 TexturePacker 등의 출력(JSON/XML)을 읽겠지만 포맷 파서는 관심사가 아니다 — 포맷을 바꿀 때 LoadFromFile 만 바꾸면 된다.
//
// [문자열 규칙] 경로는 wstring(Win32 API 가 wide), 식별자(프레임 이름)는 string(ASCII). 이 프로젝트 전체의 규칙이다.
//
// [소유] 관리자를 만들지 않는다. 아틀라스는 "장면" 수준 데이터라 지금은 Execute 가 unique_ptr 로 하나 든다. 여러 개가 필요해지는 시점(씬 시스템)에 관리자를 만든다.
class TextureAtlas final
{
public:
	// 정의 파일 형식 (한 줄 = 한 항목, '#' 로 시작하는 줄은 주석, 빈 줄 무시, 공백 구분):
	//   texture <리포 루트 상대 경로>
	//   frame <이름> <x> <y> <w> <h> [<pivotX> <pivotY>]
	// 텍스처 경로는 정의 파일이 있는 폴더 기준이 아니라 리포 루트 기준이다 (TextureManager 의 규칙과 같게).
	// 파일 없음 / texture 줄 없음 / 텍스처 로드 실패 → false. 깨진 frame 줄은 줄 번호와 함께 출력창에 찍고 그 줄만 건너뛴다.
	// [다시 부르면] 성공 시에만 내용이 통째로 교체된다 (실패하면 이전 상태 그대로). 교체되면 이전에 내준 AtlasFrame* 는 전부 무효다 —
	// 그 프레임으로 Set() 한 SpriteAnimation 도 다시 Set() 해야 한다. (핫리로드는 범위 밖이지만, 규칙은 적어 둔다)
	bool LoadFromFile(TextureManager* textures, const std::wstring& relativePath);

	Texture*          GetTexture() const { return _texture; }
	const AtlasFrame* GetFrame(const std::string& name) const;   // 없으면 nullptr
	// 접두사로 정렬된 프레임 목록 — "item_" → item_00, item_01, ... (애니메이션 구성용). 정렬은 이름의 사전순이라 번호는 자릿수를 맞춰야 한다.
	std::vector<const AtlasFrame*> GetFrames(const std::string& prefix) const;
	size_t GetFrameCount() const { return _frames.size(); }

private:
	Texture* _texture = nullptr;                 // 소유하지 않는다 (TextureManager)
	std::map<std::string, AtlasFrame> _frames;   // map 인 이유 : GetFrames 의 접두사 순회에 정렬이 필요하다. 노드가 안정적이라 AtlasFrame* 를 밖에 내줄 수 있다
};

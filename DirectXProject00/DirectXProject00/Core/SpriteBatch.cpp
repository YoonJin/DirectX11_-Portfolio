#include "pch.h"
#include "SpriteBatch.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "TextureAtlas.h"
#include <algorithm>
#include <cmath>

namespace
{
	// straight 알파 0xAABBGGRR → 프리멀티플라이드 0xAABBGGRR.
	// [함정] 프리멀티플라이드 블렌드(ONE / INV_SRC_ALPHA)에 straight 색을 그대로 넣으면 "반투명 파랑 0x80FF0000" 이
	//        실제로는 불투명 파랑 + 배경 절반 = 과하게 밝은 색이 된다. 올바른 값은 RGB 도 절반인 0x80800000 이다.
	//        호출자가 매번 이 계산을 하는 건 실수의 온상이므로, 인터페이스는 straight 로 받고 여기서 한 번만 변환한다.
	//        (텍스처 쪽은 로드 시점에 WIC 가 같은 일을 한다 — 둘 다 "입력 시점에 한 번" 이라는 같은 규칙이다)
	uint32_t PremultiplyColor(uint32_t color)
	{
		const uint32_t a = (color >> 24) & 0xFF;
		if (a == 0xFF) return color;   // 불투명이면 그대로 (가장 흔한 경우)
		// 반올림(+127) : WIC 의 PRGBA 변환 / DirectXTK 와 같은 규칙. 버림과의 차이는 최대 1/255 라 눈에 보이진 않지만,
		// 텍스처(로드 시 WIC)와 틴트(여기)가 같은 변환 규칙을 쓰는 편이 일관된다.
		const uint32_t r = (((color      ) & 0xFF) * a + 127) / 255;
		const uint32_t g = (((color >>  8) & 0xFF) * a + 127) / 255;
		const uint32_t b = (((color >> 16) & 0xFF) * a + 127) / 255;
		return (a << 24) | (b << 16) | (g << 8) | r;
	}
}

SpriteBatch::SpriteBatch(Graphics* graphics, TextureManager* textureManager, ShaderManager* shaderManager, RenderStates* renderStates)
	: _graphics(graphics), _renderStates(renderStates)
{
	// [입력 레이아웃]
	// 정점 버퍼의 바이트 배치(SpriteVertex)와 VS 의 입력 시그니처(VSIn)를 이어주는 desc. 실제 객체는 Shader 가 VS 바이트코드로 만든다.
	// color 는 R8G8B8A8_UNORM : 바이트 4개가 셰이더에서 float4(0..1) 로 자동 정규화된다. 정점 크기를 32 → 20 바이트로 줄인다.
	const std::vector<D3D11_INPUT_ELEMENT_DESC> layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(SpriteVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(SpriteVertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(SpriteVertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	// 셰이더와 흰 텍스처는 관리자에서 빌린다. 같은 파일/엔트리를 다른 사용자가 요청하면 같은 객체를 받는다.
	_shader = shaderManager->Load(L"Shaders/Sprite.hlsl", "VS", "PS", layout);
	_whiteTexture = textureManager->GetWhite();
	assert(_shader && "SpriteBatch: Shaders/Sprite.hlsl failed to compile (see debug output)");
	assert(_whiteTexture && "SpriteBatch: white texture creation failed");

	CreateBuffers();
	BuildProjection();
	_sprites.reserve(kMaxBatchSize);
	_sortedIndices.reserve(kMaxBatchSize);

	// 논리 해상도가 바뀌면 투영만 다시 만든다. 리사이즈(백버퍼 크기 변경)는 구독하지 않는다 — 헤더의 좌표계 절 참고.
	// SpriteBatch 는 Graphics 보다 먼저 죽으므로(Execute 소멸 순서) 소멸자에서 Remove 해도 안전하다.
	_logicalResolutionListenerId = _graphics->AddLogicalResolutionListener([this](uint, uint) { BuildProjection(); });
}

SpriteBatch::~SpriteBatch()
{
	_graphics->RemoveLogicalResolutionListener(_logicalResolutionListenerId);
	// 소유한 COM 객체는 ComPtr 이 선언 역순으로 놓는다. 빌린 Shader/Texture 는 관리자의 것이라 건드리지 않는다.
}

void SpriteBatch::CreateBuffers()
{
	ID3D11Device* device = _graphics->GetDevice();
	HRESULT hr = S_OK;

	// [정점 버퍼 — DYNAMIC + Map(WRITE_DISCARD)]
	// TestPattern 의 상수 버퍼는 USAGE_DEFAULT + UpdateSubresource 였다. 그 방식은 "가끔 조금" 갱신할 때 맞다.
	// flush 마다 통째로 갈아끼우는 정점 버퍼는 USAGE_DYNAMIC + CPU_ACCESS_WRITE + Map(WRITE_DISCARD) 가 정석이다.
	// WRITE_DISCARD 는 "이전 내용은 버려도 된다" 는 약속이라, 드라이버가 GPU 가 아직 읽는 중인 이전 메모리를 기다리지 않고
	// 새 메모리 블록을 내어준다(버퍼 리네이밍). UpdateSubresource 로 같은 일을 하면 GPU 와의 동기화 대기가 생길 수 있다.
	// 9단계 : 배치 하나(kMaxBatchSize 스프라이트)분으로 키웠다. 한 flush 가 이 버퍼를 한 번 채운다.
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(SpriteVertex) * 4 * kMaxBatchSize;   // 5단계의 쿼드 1개 → 배치 1개
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = device->CreateBuffer(&desc, nullptr, _vertexBuffer.GetAddressOf());
		CHECK(hr);
	}

	// [인덱스 버퍼 — IMMUTABLE]
	// 쿼드의 인덱스 패턴은 고정(0,1,2 / 0,2,3)이라 생성 시 한 번 채우고 다시는 쓰지 않는다. 9단계 : 같은 패턴을 kMaxBatchSize 번 반복해 채운다
	// (k번째 쿼드 = 패턴 + 4k). flush 는 앞에서부터 count × 6 개만 쓴다.
	// 정점 순서 : 0=좌상, 1=우상, 2=우하, 3=좌하. 두 삼각형 모두 화면 기준 시계 방향 → D3D 기본 래스터 상태(시계 = 앞면)에서도 컬링되지 않는다.
	// (7단계부터는 Begin 이 CullNone 을 명시하므로 순서에 의존하지 않는다. 회전/뒤집기가 순서를 뒤집어도 안전하다)
	// 16비트 인덱스 : 최대 정점 번호 = kMaxBatchSize × 4 - 1 = 8191 < 65536.
	{
		static_assert(kMaxBatchSize * 4 <= 65536, "16-bit index buffer cannot address this batch size");
		std::vector<uint16_t> indices(static_cast<size_t>(kMaxBatchSize) * 6);
		for (uint k = 0; k < kMaxBatchSize; ++k)
		{
			const uint16_t base = static_cast<uint16_t>(k * 4);
			uint16_t* q = &indices[static_cast<size_t>(k) * 6];
			q[0] = base + 0; q[1] = base + 1; q[2] = base + 2;
			q[3] = base + 0; q[4] = base + 2; q[5] = base + 3;
		}
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA init = {};
		init.pSysMem = indices.data();
		hr = device->CreateBuffer(&desc, &init, _indexBuffer.GetAddressOf());
		CHECK(hr);
	}

	// [상수 버퍼 — 투영 행렬]
	// 논리 해상도가 바뀔 때만 갱신되므로 DEFAULT + UpdateSubresource 로 충분하다. 64 바이트 = 16 의 배수.
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(XMFLOAT4X4);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		hr = device->CreateBuffer(&desc, nullptr, _constantBuffer.GetAddressOf());
		CHECK(hr);
	}
}

void SpriteBatch::BuildProjection()
{
	const float w = static_cast<float>(Settings::Get().GetLogicalWidth());
	const float h = static_cast<float>(Settings::Get().GetLogicalHeight());

	// 좌상단 원점, y 아래 방향 (top = 0, bottom = h) — 2D 스프라이트/UI 관례. 3D 의 y 위 방향과 반대라 top/bottom 인자 순서가 뒤집혀 있다.
	// 깊이 0..1 은 쓰지 않지만 NDC z 가 범위 안에 있어야 래스터라이저가 잘라내지 않으므로 near 0, far 1.
	const XMMATRIX projection = XMMatrixOrthographicOffCenterLH(0.f, w, h, 0.f, 0.f, 1.f);

	// [전치하는 이유]
	// HLSL 의 mul(vector, matrix) 는 row-major 벡터 × 행렬이고 DirectXMath 도 row-major 다. 그런데 HLSL 컴파일러는
	// 기본으로 cbuffer 의 행렬을 column-major 로 읽는다(메모리를 열 우선으로 해석). 그래서 CPU 에서 전치해서 올려야
	// 셰이더가 보는 행렬이 우리가 만든 행렬과 같아진다. (대안 : 셰이더에 row_major 를 붙이기 — 둘 중 하나만 해야 한다)
	XMFLOAT4X4 transposed;
	XMStoreFloat4x4(&transposed, XMMatrixTranspose(projection));
	_graphics->GetDeviceContext()->UpdateSubresource(_constantBuffer.Get(), 0, nullptr, &transposed, 0, 0);
}

void SpriteBatch::Begin(SortMode sort, BlendMode blend, SamplerMode sampler)
{
	assert(!_inBeginEnd && "SpriteBatch::Begin called twice without End");
	_inBeginEnd = true;
	_sortMode = sort;
	_sprites.clear();   // capacity 유지 — 프레임마다 재할당하지 않는다
	_stats = Stats{};

	ID3D11DeviceContext* context = _graphics->GetDeviceContext();

	// 파이프라인 상태를 전부 명시한다. 직전에 TestPattern 이 입력 레이아웃 null / 다른 셰이더 / Opaque 블렌드를 바인딩해 두었으므로
	// "이전 상태가 남아 있겠지" 에 기대지 않는다. (7단계 규칙 : 모든 드로우 주체는 자기 상태를 명시한다. 되돌리기는 없다)
	// 9단계 : 한 Begin/End 동안 상태가 바뀌지 않으므로 flush 조건은 텍스처뿐이다 — 여기서 한 번만 건다.
	_renderStates->BindBlend(blend);
	_renderStates->BindSampler(sampler, 0);              // register(s0)
	_renderStates->BindRasterizer(RasterMode::CullNone);
	const UINT stride = sizeof(SpriteVertex);
	const UINT offset = 0;
	if (_shader) _shader->Bind(context);   // 입력 레이아웃 + VS + PS
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());   // register(b0)
}

void SpriteBatch::Draw(const Texture* texture, const RECT_F& dst, const RECT_F* src, uint32_t color,
                       float rotation, XMFLOAT2 origin, float depth, SpriteEffects effects)
{
	assert(_inBeginEnd && "SpriteBatch::Draw called outside Begin/End");
	if (!_shader || !texture || !texture->GetSRV()) return;   // _shader null = Release 에서 컴파일 실패. 이전 셰이더로 그리지 않는다

	// 9단계 : 큐에 쌓기만 한다. 텍셀 → UV 변환과 프리멀티플라이는 여기서 끝내 flush 가 정점만 만들게 한다.
	SpriteInfo info;
	info.texture = texture;
	info.dst = dst;
	if (src)
	{
		const float texWidth = static_cast<float>(texture->GetWidth());
		const float texHeight = static_cast<float>(texture->GetHeight());
		info.srcUV = { src->x / texWidth, src->y / texHeight, src->width / texWidth, src->height / texHeight };
	}
	else
	{
		info.srcUV = { 0.f, 0.f, 1.f, 1.f };
	}
	info.color = PremultiplyColor(color);
	info.rotation = rotation;
	info.origin = origin;
	info.depth = depth;
	info.effects = effects;
	_sprites.push_back(info);
}

void SpriteBatch::Draw(const Texture* texture, const AtlasFrame& frame, XMFLOAT2 position, float scale, uint32_t color,
                       float rotation, SpriteEffects effects, const float* depth)
{
	// 피벗을 빼서 좌상단을 구한다. 피벗은 텍셀 단위라 scale 을 곱한다. 회전 중심(origin)도 피벗 — 발을 축으로 돈다.
	// 뒤집어도 피벗(발)은 같은 논리 좌표에 남는다 : 좌우 반전은 UV 만 바꾸고 dst 는 그대로이므로 "아래 중앙" 피벗은 자리가 안 바뀐다.
	// (정확히는 피벗이 가로 중앙일 때만 — 255 폭에 피벗 127 이면 중앙 127.5 와 0.5 텍셀 차이로 그림이 1 텍셀 × scale 만큼 옮겨 보인다.
	//  피벗이 중앙이 아닌 프레임을 뒤집을 때는 호출자가 origin 을 거울상으로 줘야 한다 — 캐릭터 스프라이트가 생기면 그때 정한다)
	const XMFLOAT2 origin = { frame.pivot.x * scale, frame.pivot.y * scale };
	const RECT_F dst = { position.x - origin.x, position.y - origin.y,
	                     frame.source.width * scale, frame.source.height * scale };
	Draw(texture, dst, &frame.source, color, rotation, origin, depth ? *depth : position.y, effects);
}

void SpriteBatch::DrawRect(const RECT_F& dst, uint32_t color)
{
	Draw(_whiteTexture, dst, nullptr, color);
}

void SpriteBatch::SortSprites()
{
	const size_t count = _sprites.size();
	_sortedIndices.resize(count);
	for (size_t i = 0; i < count; ++i) _sortedIndices[i] = i;

	// stable_sort : 같은 키(같은 텍스처 / 같은 depth)는 Draw 호출 순서를 유지한다. 헤더의 "정렬은 stable_sort on 인덱스" 참고.
	// 인덱스만 정렬하므로 SpriteInfo(48 바이트)를 옮기지 않는다.
	const SpriteInfo* sprites = _sprites.data();
	switch (_sortMode)
	{
	case SortMode::Deferred:
		break;   // 호출 순서 그대로
	case SortMode::Texture:
		// 텍스처 포인터 값으로 묶는다 — 순서 자체는 의미 없고 "같은 것끼리 연속" 이면 된다.
		// 서로 다른 객체를 가리키는 포인터의 < 는 표준상 unspecified 다 — std::less 는 전순서를 보장한다 (MSVC 에서는 같지만 규칙대로).
		std::stable_sort(_sortedIndices.begin(), _sortedIndices.end(),
		                 [sprites](size_t a, size_t b) { return std::less<const Texture*>{}(sprites[a].texture, sprites[b].texture); });
		break;
	case SortMode::BackToFront:
		// depth 작은 것(뒤)부터 → 큰 것(앞)이 나중에 그려져 위를 덮는다. depth = 발 y 면 화면 아래쪽이 앞 — y-sort 그 자체다.
		std::stable_sort(_sortedIndices.begin(), _sortedIndices.end(),
		                 [sprites](size_t a, size_t b) { return sprites[a].depth < sprites[b].depth; });
		break;
	case SortMode::FrontToBack:
		std::stable_sort(_sortedIndices.begin(), _sortedIndices.end(),
		                 [sprites](size_t a, size_t b) { return sprites[a].depth > sprites[b].depth; });
		break;
	}
}

void SpriteBatch::MakeVertices(const SpriteInfo& s, SpriteVertex* out)
{
	// UV. 뒤집기는 u0/u1 (v0/v1) 을 맞바꾼다 — 정점 위치는 그대로.
	float u0 = s.srcUV.x, u1 = s.srcUV.x + s.srcUV.width;
	float v0 = s.srcUV.y, v1 = s.srcUV.y + s.srcUV.height;
	if (HasEffect(s.effects, SpriteEffects::FlipHorizontally)) std::swap(u0, u1);
	if (HasEffect(s.effects, SpriteEffects::FlipVertically))   std::swap(v0, v1);

	// 네 모서리를 origin 기준 로컬 좌표로 만들고 → 회전 → (dst.x + origin) 으로 이동. 회전 0 이면 sin/cos 를 건너뛴다 (대부분의 스프라이트).
	// 정점 순서는 인덱스 패턴(0,1,2 / 0,2,3)과 약속된 것 : 0=좌상, 1=우상, 2=우하, 3=좌하.
	// UV 의 v 는 텍스처 위쪽이 0 이고 논리 좌표의 y 도 위쪽이 0 이라 뒤집을 필요가 없다.
	const float lx0 = -s.origin.x, ly0 = -s.origin.y;
	const float lx1 = s.dst.width - s.origin.x, ly1 = s.dst.height - s.origin.y;
	const float cx = s.dst.x + s.origin.x, cy = s.dst.y + s.origin.y;   // 회전 중심의 논리 좌표
	const float corners[4][2] = { { lx0, ly0 }, { lx1, ly0 }, { lx1, ly1 }, { lx0, ly1 } };
	const float uvs[4][2] = { { u0, v0 }, { u1, v0 }, { u1, v1 }, { u0, v1 } };

	if (s.rotation == 0.f)
	{
		for (int i = 0; i < 4; ++i)
			out[i] = { { cx + corners[i][0], cy + corners[i][1] }, { uvs[i][0], uvs[i][1] }, s.color };
	}
	else
	{
		// y 가 아래로 가는 좌표계에서 표준 회전 행렬 [c -s; s c] 를 그대로 쓰면 양의 각이 화면상 시계 방향이다.
		const float c = std::cos(s.rotation), sn = std::sin(s.rotation);
		for (int i = 0; i < 4; ++i)
		{
			const float x = corners[i][0], y = corners[i][1];
			out[i] = { { cx + x * c - y * sn, cy + x * sn + y * c }, { uvs[i][0], uvs[i][1] }, s.color };
		}
	}
}

void SpriteBatch::Flush(const size_t* indices, size_t count)
{
	assert(count > 0 && count <= kMaxBatchSize);
	ID3D11DeviceContext* context = _graphics->GetDeviceContext();

	// Map(WRITE_DISCARD) : 이전 내용을 버리고 새 메모리를 받는다. flush 마다 불러도 GPU 와 충돌하지 않는다 (헤더 참고).
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = context->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	CHECK(hr);
	if (FAILED(hr)) return;
	SpriteVertex* vertices = static_cast<SpriteVertex*>(mapped.pData);
	for (size_t i = 0; i < count; ++i)
		MakeVertices(_sprites[indices[i]], vertices + i * 4);
	context->Unmap(_vertexBuffer.Get(), 0);

	// 이 구간은 전부 같은 텍스처다 (End 가 그렇게 잘랐다).
	ID3D11ShaderResourceView* srv = _sprites[indices[0]].texture->GetSRV();
	context->PSSetShaderResources(0, 1, &srv);   // register(t0)
	context->DrawIndexed(static_cast<UINT>(count * 6), 0, 0);
	++_stats.drawCalls;
}

void SpriteBatch::End()
{
	assert(_inBeginEnd && "SpriteBatch::End called without Begin");
	_inBeginEnd = false;

	_stats.sprites = static_cast<uint>(_sprites.size());
	_stats.batches = 1;
	if (!_sprites.empty())
	{
		SortSprites();

		// 정렬 순서대로 순회하며 "같은 텍스처" 구간을 모은다. 텍스처가 바뀌거나 kMaxBatchSize 에 닿으면 flush.
		const size_t* order = _sortedIndices.data();
		size_t begin = 0;
		for (size_t i = 1; i <= _sortedIndices.size(); ++i)
		{
			const bool last = (i == _sortedIndices.size());
			const bool textureChanged = !last && _sprites[order[i]].texture != _sprites[order[begin]].texture;
			const bool full = (i - begin) >= kMaxBatchSize;
			if (last || textureChanged || full)
			{
				Flush(order + begin, i - begin);
				if (!last)
				{
					if (textureChanged) ++_stats.flushesByTexture;
					else ++_stats.flushesByCapacity;
				}
				begin = i;
			}
		}
	}

	_frameStats.drawCalls += _stats.drawCalls;
	_frameStats.sprites += _stats.sprites;
	_frameStats.flushesByTexture += _stats.flushesByTexture;
	_frameStats.flushesByCapacity += _stats.flushesByCapacity;
	_frameStats.batches += 1;

	// 블렌드 상태는 되돌리지 않는다 (7단계) — 모든 드로우 주체가 자기 상태를 명시한다. SRV 만 풀어 둔다 — 리소스 바인딩이지 상태가 아니다.
	// (같은 텍스처를 나중에 렌더 타겟으로 쓰는 경우 디버그 레이어가 "SRV 와 RTV 동시 바인딩" 경고를 낸다)
	ID3D11ShaderResourceView* nullSrv = nullptr;
	_graphics->GetDeviceContext()->PSSetShaderResources(0, 1, &nullSrv);
}

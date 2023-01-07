#pragma once
#include"SpriteCommon.h"
#include <DirectXMath.h>

using namespace DirectX;


// 頂点データ構造体
struct Vertex
{
	XMFLOAT3 pos; // xyz座標
	XMFLOAT2 uv;  // uv座標
};
//スプライト
class Sprite {
public:
	// 定数バッファ用データ構造体（マテリアル）
	struct ConstBufferDataMaterial {
		XMFLOAT4 color; // 色 (RGBA)
	};
	//定数バッファ用構造体（３D変換行列）
	struct ConstBufferDataTransform {
		XMMATRIX mat;	//3D変換行列
	};

	//頂点番号
	enum VertexNumber {
		LB,//左下
		LT,//左上
		RB,//右下
		RT,//右上
	};
public:
	//初期化
	void Initialize(SpriteCommon* spritecommon_);

	void Draw();

	void Update();

	void SetPozition(const XMFLOAT2& position_);

	const XMFLOAT2& GetPosition() const { return position; }

	void SetRotation(float rotation_);

	float GetRotation() { return rotation; }

	void SetColor(const XMFLOAT4& color_) { color = color_; }

	XMFLOAT4 GetColor() { return color; }

	void SetScale(const XMFLOAT3& scale_) { scale = scale_; }

	XMFLOAT3 GetScale() { return scale; }

	void SetAnchorPoint(const XMFLOAT2& anchorPoint_) { anchorPoint = anchorPoint_; }

	XMFLOAT2 GetAnchorPoint() { return anchorPoint; }

	void SetTextureIndex(uint32_t texNmb) { textureIndex_ = texNmb; }

	uint32_t GetTextureIndex() { return textureIndex_; }

	void SetTexSize(XMFLOAT2 texSize) { textureSize = texSize; }

	XMFLOAT2 GetTexSize() { return textureSize; }

	XMFLOAT2 GetSize() { return size_; }

	void SetSize(XMFLOAT2 size);

	/// 上下反転の設定
	void SetIsFlipY(bool isFlipY);

	/// 左右反転の設定
	void SetIsFlipX(bool isFlipX);

private:
	SpriteCommon* spritecomon;
	HRESULT result;
	// 頂点データ
	Vertex vertices[4] = {
		// x      y     z       u     v
		{{-0.4f, -0.7f, 0.0f}, {0.0f, 1.0f}}, // 左下
		{{-0.4f, +0.7f, 0.0f}, {0.0f, 0.0f}}, // 左上
		{{+0.4f, -0.7f, 0.0f}, {1.0f, 1.0f}}, // 右下
		{{+0.4f, +0.7f, 0.0f}, {1.0f, 0.0f}}, // 右上
	};

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};

	XMMATRIX matScale;//スケーリング行列
	XMMATRIX matWorld;
	XMMATRIX matRot;//回転行列
	XMMATRIX  matTrans;//平行移動行列

	//座標
	XMFLOAT3 scale{ 0.5f, 0.5f, 1.0f };

	XMFLOAT2 size_ = { 100.0f,100.0f };

	float rotation = 0.0f;
	XMFLOAT2 position = { 0.0f, 0.0f };

	XMFLOAT4 color = { 1,1,1,1 };

	XMFLOAT2 anchorPoint = { 0.0f,0.0f };

	ID3D12Resource* constBuffTransform = nullptr;
	ConstBufferDataTransform* constMapTransform = nullptr;

	ConstBufferDataMaterial* constMapMaterial = nullptr;

	ID3D12Resource* constBuffMaterial = nullptr;

	Vertex vertices_[4];

	Vertex* vertMap = nullptr;

	//テクスチャ番号
	uint32_t textureIndex_ = 0;

	//テクスチャ左上座標
	XMFLOAT2 textureLeftTop = { 0.0f,0.0f };
	//テクスチャ切り出しサイズ
	XMFLOAT2 textureSize = { 100.0f,100.0f };

	XMMATRIX matProjection;

	// アンカーポイント
	XMFLOAT2 anchorpoint = { 0, 0 };

	// 頂点バッファの生成
	ID3D12Resource* vertBuff = nullptr;

	// 左右反転
	bool isFlipX = false;
	// 上下反転
	bool isFlipY = false;
};


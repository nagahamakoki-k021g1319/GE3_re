#pragma region 読み込むヘッダー

#define DIRECTINPUT_VERSION 0x0800 //DirectInputのバージョン指定
#include<dinput.h>
#include<d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")
#include<Windows.h>
#include <tchar.h>
#include<iostream>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#include <vector>
#include <string>
#include<DirectXMath.h>
#include <math.h>
#include <DirectXTex.h>
#include<wrl.h>
#include "Input.h"
#include "WinApp.h"



using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;


const float PI = 3.141592f;

// 定数バッファ用データ構造体（マテリアル）
struct ConstBufferDataMaterial {
	XMFLOAT4 color; // 色 (RGBA)
};
// 定数バッファ用データ構造体Pos（マテリアル）
struct ConstBufferDataMaterialPos {
	XMFLOAT4 Move; // 移動
};
//定数バッファ用データ構造体(3D変換行列)
struct ConstBufferDataTransform {
	XMMATRIX mat;
};

//3Dオブジェクト
struct Object3d
{
	//定数バッファ(行列用)
	ComPtr<ID3D12Resource> constBuffTransform;
	//定数バッファマップ
	ConstBufferDataTransform* constMapTransform = nullptr;
	//アフィン変換情報
	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotation = { 0,0,0 };
	XMFLOAT3 position = { 0,0,0 };
	//ワールド変換行列
	XMMATRIX matWorld{};
	//親オブジェクトへのポインタ
	Object3d* parent = nullptr;
};

//3Dオブジェクト初期化
void InitializeObject3d(Object3d* object, ID3D12Device* device) {

	HRESULT result;

	//定数バッファのヒープ設定
	D3D12_HEAP_PROPERTIES heapProp{};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	//定数バッファのリソース設定
	D3D12_RESOURCE_DESC resdesc{};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = (sizeof(ConstBufferDataTransform) + 0xff) & ~0xff;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.SampleDesc.Count = 1;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//定数バッファの生成
	result = device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&object->constBuffTransform));
	assert(SUCCEEDED(result)
	);
	//定数バッファのマッピング
	result = object->constBuffTransform->Map(0, nullptr, (void**)&object->constMapTransform);
	assert(SUCCEEDED(result));
}

void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection) {

	XMMATRIX matScale, matRot, matTrans;

	//スケール、回転、平行移動行列の計算
	matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
	matRot = XMMatrixIdentity();
	matRot *= XMMatrixRotationZ(object->rotation.z);
	matRot *= XMMatrixRotationX(object->rotation.x);
	matRot *= XMMatrixRotationY(object->rotation.y);
	matTrans = XMMatrixTranslation(object->position.x, object->position.y, object->position.z);

	//ワールド行列の合成
	object->matWorld = XMMatrixIdentity();
	object->matWorld *= matScale;
	object->matWorld *= matRot;
	object->matWorld *= matTrans;

	//親オブジェクトがあれば
	if (object->parent != nullptr) {
		//親オブジェクトのワールド行列をかける
		object->matWorld *= object->parent->matWorld;
	}

	//定数バッファへデータ転送
	object->constMapTransform->mat = object->matWorld * matView * matProjection;
}

void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* commandList, D3D12_VERTEX_BUFFER_VIEW& vbView, D3D12_INDEX_BUFFER_VIEW& ibView, UINT numIndices) {
	//頂点バッファの設定
	commandList->IASetVertexBuffers(0, 1, &vbView);
	//インデックスバッファの設定
	commandList->IASetIndexBuffer(&ibView);
	//定数バッファビュー(CBV)の設定コマンド
	commandList->SetGraphicsRootConstantBufferView(2, object->constBuffTransform->GetGPUVirtualAddress());
	//描画コマンド
	commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}






#pragma region おまじない
// @brief コンソール画面にフォーマット付き文字列の表示
// @param format フォーマット(%dとか%fとかの)
// @param 可変長引数
// @remarks この関数はデバック用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//ウィンドウが破壊されたら呼ばれる
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);//OSに対して「このアプリはもう終わる」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
#pragma endregion

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

#pragma region WindowsAPI初期化処理

	//ポインタ
	WinApp* winApp = nullptr;

	//windowsAPIの初期化
	winApp = new WinApp();
	winApp->Initialize();

	MSG msg = {};

#pragma endregion

#pragma region DirectX初期化処理

	////////////////////////////////////////////////////
	//-------------DirectX12初期化処理ここから-----------//
	//////////////////////////////////////////////////

#ifdef _DEBUG
//デバッグレイヤーをオンに
	ComPtr < ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
	}
#endif

	HRESULT result;
	ComPtr <ID3D12Device> device;
	ComPtr <IDXGIFactory6> dxgiFactory;
	ComPtr <IDXGISwapChain4> swapChain;
	ComPtr <ID3D12CommandAllocator> commandAllocator;
	ComPtr <ID3D12GraphicsCommandList> commandList;
	ComPtr <ID3D12CommandQueue> commandQueue;
	ComPtr <ID3D12DescriptorHeap> rtvHeap;

	//DXGIファクトリーの生成
	result = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(result));

	//アダプターの列挙用
	std::vector< ComPtr<IDXGIAdapter4>> adapters;
	//ここに特定の名前を持つアダプターオブジェクトが入る
	ComPtr < IDXGIAdapter4> tmpAdapter;

	//パフォーマンスが高い物から順に、全てのアダプターを列挙する
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&tmpAdapter)) != DXGI_ERROR_NOT_FOUND; i++) {
		//動的配列に追加する
		adapters.push_back(tmpAdapter);
	}
	//妥当なアダプタを選別する
	for (size_t i = 0; i < adapters.size(); i++) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		//アダプターの情報を取得する
		adapters[i]->GetDesc3(&adapterDesc);

		//ソフトウェアデバイスを回避
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			//デバイスを採用してループを抜ける
			tmpAdapter = adapters[i].Get();
			break;
		}
	}
	//対応レベルの配列
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;

	for (size_t i = 0; i < _countof(levels); i++) {
		//採用したアダプターでデバイスを生成
		result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&device));
		if (result == S_OK) {
			//デバイスを生成できた時点でループを抜ける
			featureLevel = levels[i];
			break;
		}
	}

	//コマンドアローケータを生成
	result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(result));

	//コマンドリストを生成
	result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(result));

	//コマンドキューの設定
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	//コマンドキューを生成
	result = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(result));

	//スワップチェーンの設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = 1280;
	swapChainDesc.Height = 720;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			//色情報の書式
	swapChainDesc.SampleDesc.Count = 1;							//マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;			//バックバッファ用
	swapChainDesc.BufferCount = 2;								//バッファ数を2つに設定
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	//フリップ後は破壊
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	
	//IDXGISwapChain1のComPtrを用意
	ComPtr<IDXGISwapChain1> swapchain1;

	//スワップチェーンの生成
	result = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(), 
		winApp->GetHwnd(),
		&swapChainDesc, 
		nullptr, 
		nullptr, 
		&swapchain1);
	
	//生成したIDXGISwapChain1のオブジェクトをIDXGISwapChain4に変換する
	swapchain1.As(&swapChain);

	//デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;			//レンダーターゲットビュー
	rtvHeapDesc.NumDescriptors = swapChainDesc.BufferCount;		//裏表の2つ
	//デスクリプタヒープの生成
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	//深度バッファのリソース設定
	D3D12_RESOURCE_DESC depthResourceDesc{};
	depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResourceDesc.Width = WinApp::window_width;
	depthResourceDesc.Height = WinApp::window_height;
	depthResourceDesc.DepthOrArraySize = 1;
	depthResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResourceDesc.SampleDesc.Count = 1;
	depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	//深度用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp{};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	//深度バッファのリソース生成
	ComPtr < ID3D12Resource> depthBuff;
	result = device->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthBuff)
	);
	//深度ビュー用デスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ComPtr < ID3D12DescriptorHeap> dsvHeap;
	result = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(
		depthBuff.Get(),
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart()
	);


	//バックバッファ
	std::vector< ComPtr<ID3D12Resource>> backBuffers;
	backBuffers.resize(swapChainDesc.BufferCount);

	//スワップチェーンの全てのバッファについて処理する
	for (size_t i = 0; i < backBuffers.size(); i++) {
		//スワップチェーンからバッファを取得
		swapChain->GetBuffer((UINT)i, IID_PPV_ARGS(&backBuffers[i]));
		//デスクリプタヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		//裏か表かでアドレスがずれる
		rtvHandle.ptr += i * device->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);
		//レンダーターゲットビューの設定
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		//シェーダーの計算結果をSRGBに変換して書き込む
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		//レンダーターゲットビューの生成
		device->CreateRenderTargetView(backBuffers[i].Get(), &rtvDesc, rtvHandle);
	}

	//フェンスの生成
	ComPtr < ID3D12Fence> fence = nullptr;
	UINT64 fenceVal = 0;

	result = device->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	//ポインタ
	Input* input = nullptr;

	//入力の初期化　
	input = new Input();
	input->Initialize(winApp);

	//3Dオブジェクトの数
	const size_t kObjectCount = 50;
	//3Dオブジェクトの配列
	Object3d object3ds[kObjectCount];

	/////////////////////////////////////////////////////////
	//--------------DirectX12初期化処理　ここまで-------------//
	///////////////////////////////////////////////////////
#pragma endregion

#pragma region 描画初期化処理

	//////////////////////////////////////////////////////
	//---------------描画初期化処理 ここから----------------//
	/////////////////////////////////////////////////////

	TexMetadata metadata{};
	ScratchImage scratchImg{};
	// WICテクスチャのロード
	result = LoadFromWICFile(
		L"Resources/bb.png",   //「Resources」フォルダの「texture.png」
		WIC_FLAGS_NONE,
		&metadata, scratchImg);
	ScratchImage mipChain{};
	// ミップマップ生成
	result = GenerateMipMaps(
		scratchImg.GetImages(), scratchImg.GetImageCount(), scratchImg.GetMetadata(),
		TEX_FILTER_DEFAULT, 0, mipChain);
	if (SUCCEEDED(result)) {
		scratchImg = std::move(mipChain);
		metadata = scratchImg.GetMetadata();
	}
	// 読み込んだディフューズテクスチャをSRGBとして扱う
	metadata.format = MakeSRGB(metadata.format);

	//2枚目用の別の変数を用意
	TexMetadata metadata2{};
	ScratchImage scratchImg2{};
	// WICテクスチャのロード
	result = LoadFromWICFile(
		L"Resources/reimu.png",   //「Resources」フォルダの「texture.png」
		WIC_FLAGS_NONE,
		&metadata2, scratchImg2);
	ScratchImage mipChain2{};
	// ミップマップ生成
	result = GenerateMipMaps(
		scratchImg2.GetImages(), scratchImg2.GetImageCount(), scratchImg2.GetMetadata(),
		TEX_FILTER_DEFAULT, 0, mipChain2);
	if (SUCCEEDED(result)) {
		scratchImg2 = std::move(mipChain2);
		metadata2 = scratchImg2.GetMetadata();
	}
	// 読み込んだディフューズテクスチャをSRGBとして扱う
	metadata2.format = MakeSRGB(metadata2.format);

	// ヒープ設定
	D3D12_HEAP_PROPERTIES textureHeapProp{};
	textureHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	textureHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	textureHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	//// リソース設定
	//D3D12_RESOURCE_DESC textureResourceDesc{};
	//textureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; textureResourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; textureResourceDesc.Width = textureWidth;  // 幅
	//textureResourceDesc.Height = textureWidth; // 高さ
	//textureResourceDesc.DepthOrArraySize = 1;
	//textureResourceDesc.MipLevels = 1;
	//textureResourceDesc.SampleDesc.Count = 1;

	D3D12_RESOURCE_DESC textureResourceDesc{};
	textureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureResourceDesc.Format = metadata.format;
	textureResourceDesc.Width = metadata.width;
	textureResourceDesc.Height = (UINT)metadata.height;
	textureResourceDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
	textureResourceDesc.MipLevels = (UINT16)metadata.mipLevels;
	textureResourceDesc.SampleDesc.Count = 1;

	D3D12_RESOURCE_DESC textureResourceDesc2{};
	textureResourceDesc2.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureResourceDesc2.Format = metadata2.format;
	textureResourceDesc2.Width = metadata2.width;
	textureResourceDesc2.Height = (UINT)metadata2.height;
	textureResourceDesc2.DepthOrArraySize = (UINT16)metadata2.arraySize;
	textureResourceDesc2.MipLevels = (UINT16)metadata2.mipLevels;
	textureResourceDesc2.SampleDesc.Count = 1;

	// テクスチャバッファの生成
	ComPtr < ID3D12Resource> texBuff;
	result = device->CreateCommittedResource(
		&textureHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&textureResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff));

	// テクスチャバッファの生成2
	ComPtr < ID3D12Resource> texBuff2;
	result = device->CreateCommittedResource(
		&textureHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&textureResourceDesc2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff2));

	// テクスチャバッファにデータ転送
	//result = texBuff->WriteToSubresource(
	//	0,
	//	nullptr, // 全領域へコピー
	//	imageData,    // 元データアドレス
	//	sizeof(XMFLOAT4) * textureWidth, // 1ラインサイズ
	//	sizeof(XMFLOAT4) * imageDataCount // 全サイズ
	//);
	// 全ミップマップについて
	for (size_t i = 0; i < metadata.mipLevels; i++) {
		// ミップマップレベルを指定してイメージを取得
		const Image* img = scratchImg.GetImage(i, 0, 0);
		// テクスチャバッファにデータ転送
		result = texBuff->WriteToSubresource(
			(UINT)i,
			nullptr,              // 全領域へコピー
			img->pixels,          // 元データアドレス
			(UINT)img->rowPitch,  // 1ラインサイズ
			(UINT)img->slicePitch // 1枚サイズ
		);
		assert(SUCCEEDED(result));
	}
	// 全ミップマップについて
	for (size_t i = 0; i < metadata2.mipLevels; i++) {
		// ミップマップレベルを指定してイメージを取得
		const Image* img2 = scratchImg2.GetImage(i, 0, 0);
		// テクスチャバッファにデータ転送
		result = texBuff2->WriteToSubresource(
			(UINT)i,
			nullptr,              // 全領域へコピー
			img2->pixels,          // 元データアドレス
			(UINT)img2->rowPitch,  // 1ラインサイズ
			(UINT)img2->slicePitch // 1枚サイズ
		);
		assert(SUCCEEDED(result));
	}


	// SRVの最大個数
	const size_t kMaxSRVCount = 2056;

	// デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	srvHeapDesc.NumDescriptors = kMaxSRVCount;

	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
	// 設定を元にSRV用デスクリプタヒープを生成
    ComPtr<ID3D12DescriptorHeap> srvHeap;
	result = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap));
	assert(SUCCEEDED(result));

	



	//// 頂点データ
//XMFLOAT3 vertices[] = {
//{ -0.5f, -0.5f, 0.0f }, // 左下 インデックス0
//{ -0.5f, +0.5f, 0.0f }, // 左上 インデックス1
//{ +0.5f, -0.5f, 0.0f }, // 右下 インデックス2
//{ +0.5f, +0.5f, 0.0f }, // 右上 インデックス3
//};

// 頂点データ構造体
	struct Vertex
	{
		XMFLOAT3 pos; // xyz座標
		XMFLOAT3 normal; //法線ベクトル
		XMFLOAT2 uv;  // uv座標
	};
	// 頂点データ
	Vertex vertices[] = {
		//  x        y     z       u     v
		 //前
		{{-5.0f, -5.0f, -5.0f},{} ,{0.0f, 1.0f}}, // 左下
		{{-5.0f, 5.0f, -5.0f},{} , {0.0f, 0.0f}}, // 左上
		{{5.0f, -5.0f, -5.0f},{} , {1.0f, 1.0f}}, // 右下
		{{5.0f, 5.0f, -5.0f}, {} ,{1.0f, 0.0f}}, // 右上
		//後ろ
		{{-5.0f, -5.0f, 5.0f}, {} ,{0.0f, 1.0f}}, // 左下
		{{-5.0f, 5.0f, 5.0f},{} , {0.0f, 0.0f}}, // 左上
		{{5.0f, -5.0f, 5.0f}, {} ,{1.0f, 1.0f}}, // 右下
		{{5.0f, 5.0f, 5.0f},{} , {1.0f, 0.0f}}, // 右上
		//左
		{{-5.0f, -5.0f, -5.0f},{} , {0.0f, 1.0f}}, // 左下
		{{-5.0f, -5.0f, 5.0f},{} , {0.0f, 0.0f}}, // 左上
		{{-5.0f, 5.0f, -5.0f},{} , {1.0f, 1.0f}}, // 右下
		{{-5.0f, 5.0f, 5.0f}, {} ,{1.0f, 0.0f}}, // 右上
		//右
		{{5.0f, -5.0f, -5.0f},{} , {0.0f, 1.0f}}, // 左下
		{{5.0f, -5.0f, 5.0f},{} , {0.0f, 0.0f}}, // 左上
		{{5.0f, 5.0f, -5.0f}, {} ,{1.0f, 1.0f}}, // 右下
		{{5.0f, 5.0f, 5.0f},{} , {1.0f, 0.0f}}, // 右上
		//下
		{{-5.0f, -5.0f, -5.0f},{} , {0.0f, 1.0f}}, // 左下
		{{-5.0f, -5.0f, 5.0f}, {} ,{0.0f, 0.0f}}, // 左上
		{{5.0f, -5.0f, -5.0f}, {} ,{1.0f, 1.0f}}, // 右下
		{{5.0f, -5.0f, 5.0f}, {} ,{1.0f, 0.0f}}, // 右上
		//上
		{{-5.0f, 5.0f, -5.0f}, {} ,{0.0f, 1.0f}}, // 左下
		{{-5.0f, 5.0f, 5.0f}, {} ,{0.0f, 0.0f}}, // 左上
		{{5.0f, 5.0f, -5.0f}, {} ,{1.0f, 1.0f}}, // 右下
		{{5.0f, 5.0f, 5.0f},{} , {1.0f, 0.0f}}, // 右上
	};

	//// インデックスデータ
	//uint16_t indices[] =
	//{
	//	0, 1, 2, // 三角形1つ目
	//	1, 2, 3, // 三角形2つ目
	//};

	// インデックスデータ
	// インデックスデータ
	unsigned short indices[] = {
		//前
		0, 1, 2, // 三角形1つ目
		3, 2, 1, // 三角形2つ目
		//後
		6, 5, 4, // 三角形3つ目
		5, 6, 7, // 三角形4つ目
		//左
		8, 9, 10, // 三角形5つ目
		11, 10, 9, // 三角形6つ目
		//右
		14, 13, 12, // 三角形7つ目
		13, 14, 15, // 三角形8つ目
		//下
		18, 17, 16, // 三角形9つ目
		17, 18, 19, // 三角形10つ目
		//上
		20, 21, 22, // 三角形11つ目
		23, 22, 21, // 三角形12つ目
	};


	/*float transformX = 0.0f;
	float transformY = 0.0f;
	float rotation = 0.0f;
	float scale = 1.0f;
	float affin[3][3] = {
		{1.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 1.0f}
	};*/


	// 頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
	/*UINT sizeVB = static_cast<UINT>(sizeof(XMFLOAT3) * _countof(vertices));*/
	UINT sizeVB = static_cast<UINT>(sizeof(vertices[0]) * _countof(vertices));
	// 頂点バッファの設定
	D3D12_HEAP_PROPERTIES heapProp{}; // ヒープ設定
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへの転送用
	// リソース設定
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeVB; // 頂点データ全体のサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// 頂点バッファの生成
	ComPtr < ID3D12Resource> vertBuff;
	result = device->CreateCommittedResource(
		&heapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&resDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));
	assert(SUCCEEDED(result));

	// インデックスデータ全体のサイズ
	UINT sizeIB = static_cast<UINT>(sizeof(uint16_t) * _countof(indices));

	// リソース設定
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeIB; // インデックス情報が入る分のサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// インデックスバッファの生成
	ComPtr < ID3D12Resource> indexBuff;
	result = device->CreateCommittedResource(
		&heapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&resDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff));

	// インデックスバッファをマッピング
	uint16_t* indexMap = nullptr;
	result = indexBuff->Map(0, nullptr, (void**)&indexMap);
	// 全インデックスに対して
	for (int i = 0; i < _countof(indices); i++)
	{
		indexMap[i] = indices[i];   // インデックスをコピー
	}
	// マッピング解除
	indexBuff->Unmap(0, nullptr);

	// インデックスバッファビューの作成
	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	//法線計算
	for (int i = 0; i < _countof(indices) / 3; i++)
	{//三角形1つごとに計算していく
		//三角形のインデックスを取り出して一時的な変数に入れる
		unsigned short indices_0 = indices[i * 3 + 0];
		unsigned short indices_1 = indices[i * 3 + 1];
		unsigned short indices_2 = indices[i * 3 + 2];
		//三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[indices_0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[indices_1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[indices_2].pos);
		//p0→p1ベクトル,p0→p2ベクトルを計算 (ベクトルの減算)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);
		//外積は両方から垂直なベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);
		//正規化(長さを1にする)
		normal = XMVector3Normalize(normal);
		//求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[indices_0].normal, normal);
		XMStoreFloat3(&vertices[indices_1].normal, normal);
		XMStoreFloat3(&vertices[indices_2].normal, normal);
	}


	// GPU上のバッファに対応した仮想メモリ(メインメモリ上)を取得
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	assert(SUCCEEDED(result));

	// 全頂点に対して
	for (int i = 0; i < _countof(vertices); i++) {
		vertMap[i] = vertices[i]; // 座標をコピー
	}
	// 繋がりを解除
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	// GPU仮想アドレス
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	// 頂点バッファのサイズ
	vbView.SizeInBytes = sizeVB;
	// 頂点1つ分のデータサイズ
	/*vbView.StrideInBytes = sizeof(XMFLOAT3);*/
	vbView.StrideInBytes = sizeof(vertices[0]);

	//
	// 
	// 
	// 
	// 
	// 
	// 
	// 
	//SRVヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
	// シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = resDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = resDesc.MipLevels;

	// ハンドルの指す位置にシェーダーリソースビュー作成
	device->CreateShaderResourceView(texBuff.Get(), &srvDesc, srvHandle);

	//1つハンドルを進める
	UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	srvHandle.ptr += incrementSize;

	//シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = textureResourceDesc2.Format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = textureResourceDesc2.MipLevels;

	// ハンドルの指す位置にシェーダーリソースビュー作成
	device->CreateShaderResourceView(texBuff2.Get(), &srvDesc2, srvHandle);

	// CBV,SRV,UAVの1個分のサイズを取得
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// ハンドルを1つ進める（SRVの位置）
	srvHandle.ptr += descriptorSize * 1;

	// CBV(コンスタントバッファビュー)の設定
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	//cbvDescの値設定（省略）
	// 定数バッファビュー生成
	device->CreateConstantBufferView(&cbvDesc, srvHandle);

	// デスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.NumDescriptors = 1;         //一度の描画に使うテクスチャが1枚なので1
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.BaseShaderRegister = 0;     //テクスチャレジスタ0番
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;





	//定数バッファ(0)
	ComPtr < ID3D12Resource> constBuffTransform0;
	ConstBufferDataTransform* constMapTransform0 = nullptr;
	//定数バッファ(1)
	ComPtr < ID3D12Resource> constBuffTransform1;
	ConstBufferDataTransform* constMapTransform1 = nullptr;

	

	////スケーリング倍率
	//XMFLOAT3 scale = { 1.0f,1.0f,1.0f };
	////回転角
	//XMFLOAT3 rotation = { 0.0f,0.0f,0.0f };
	////座標
	//XMFLOAT3 position = { 0.0f,0.0f,0.0f };

	////ワールド変換行列
	//XMMATRIX matWorld;
	////単位行列を代入
	//matWorld = XMMatrixIdentity();
	//
	//XMMATRIX matScale; //スケーリング行列
	//XMMATRIX matRot;
	//XMMATRIX matTrans;

	//matTrans = XMMatrixTranslation(-50.0f, 0.0f, 0.0f);
	//matWorld *= matTrans;

	{
		// ヒープ設定
		D3D12_HEAP_PROPERTIES cbHeapProp{};
		cbHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;                   // GPUへの転送用
		// リソース設定
		D3D12_RESOURCE_DESC cbResourceDesc{};
		cbResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		cbResourceDesc.Width = (sizeof(ConstBufferDataTransform) + 0xff) & ~0xff;   // 256バイトアラインメント
		cbResourceDesc.Height = 1;
		cbResourceDesc.DepthOrArraySize = 1;
		cbResourceDesc.MipLevels = 1;
		cbResourceDesc.SampleDesc.Count = 1;
		cbResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		//
		// 
		// 
		// 
		//// 定数バッファの生成 (0)
		//result = device->CreateCommittedResource(
		//	&cbHeapProp, // ヒープ設定
		//	D3D12_HEAP_FLAG_NONE,
		//	&cbResourceDesc, // リソース設定
		//	D3D12_RESOURCE_STATE_GENERIC_READ,
		//	nullptr,
		//	IID_PPV_ARGS(&constBuffTransform0));
		//assert(SUCCEEDED(result));
		//// 定数バッファのマッピング
		//result = constBuffTransform0->Map(0, nullptr, (void**)&constMapTransform0); // マッピング
		//assert(SUCCEEDED(result));

		//// 定数バッファの生成 (1)
		//result = device->CreateCommittedResource(
		//	&cbHeapProp, // ヒープ設定
		//	D3D12_HEAP_FLAG_NONE,
		//	&cbResourceDesc, // リソース設定
		//	D3D12_RESOURCE_STATE_GENERIC_READ,
		//	nullptr,
		//	IID_PPV_ARGS(&constBuffTransform1));
		//assert(SUCCEEDED(result));
		//// 定数バッファのマッピング
		//result = constBuffTransform1->Map(0, nullptr, (void**)&constMapTransform1); // マッピング
		//assert(SUCCEEDED(result));

		//配列内の全オブジェクトに対して
		for (int i = 0; i < _countof(object3ds); i++)
		{
			//初期化
			InitializeObject3d(&object3ds[i], device.Get());

			//ここから下は親子構造のサンプル
			//先頭以外なら
			if (i > 0) {
				//ひとつ前のオブジェクトを親オブジェクトとする
				object3ds[i].parent = &object3ds[i - 1];
				//親オブジェクトの9割の大きさ
				object3ds[i].scale = { 0.9f,0.9f,0.9f };
				//オブジェクトに対してZ軸まわりに30度回転
				object3ds[i].rotation = { 0.0f,0.0f,XMConvertToRadians(30.0f) };

				//親オブジェクトに対してZ方向-8.0ずらす
				object3ds[i].position = { 0.0f,0.0f,-8.0f };
			}

		}



	}
	//単位行列を代入
	/*constMapTransform->mat = XMMatrixIdentity();
	constMapTransform->mat.r[0].m128_f32[0] = 2.0f / window_width;
	constMapTransform->mat.r[1].m128_f32[1] = -2.0f / window_height;
	constMapTransform->mat.r[3].m128_f32[0] = -1.0f;
	constMapTransform->mat.r[3].m128_f32[1] = 1.0f;*/
	//平行
	//constMapTransform0->mat = XMMatrixOrthographicOffCenterLH(
	//	//左端			//右端    
	//	0.0f,			 window_width,

	//	//下端			//上端
	//	window_height,  0.0f,

	//	//前端			//奥端
	//	 0.0f,			1.0f      
	//);
	//透視投影行列の計算
	XMMATRIX matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(45.0f),
		(float)WinApp::window_width / WinApp::window_height,
		0.1f, 1000.0f
	);



	//ビュー変換行列
	XMMATRIX matView;
	XMFLOAT3 eye(0, 0, -100);
	XMFLOAT3 target(0, 0, 0);
	XMFLOAT3 up(0, 1, 0);
	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	//constMapTransform0->mat = matWorld * matView * matProjection;




// ヒープ設定
	D3D12_HEAP_PROPERTIES cbHeapProp{};
	cbHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;                   // GPUへの転送用
	// リソース設定
	D3D12_RESOURCE_DESC cbResourceDesc{};
	cbResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbResourceDesc.Width = (sizeof(ConstBufferDataMaterial) + 0xff) & ~0xff;   // 256バイトアラインメント
	cbResourceDesc.Height = 1;
	cbResourceDesc.DepthOrArraySize = 1;
	cbResourceDesc.MipLevels = 1;
	cbResourceDesc.SampleDesc.Count = 1;
	cbResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr < ID3D12Resource> constBuffMaterial;
	// 定数バッファの生成
	result = device->CreateCommittedResource(
		&cbHeapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&cbResourceDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuffMaterial));
	assert(SUCCEEDED(result));

	// 定数バッファのマッピング
	ConstBufferDataMaterial* constMapMaterial = nullptr;
	result = constBuffMaterial->Map(0, nullptr, (void**)&constMapMaterial); // マッピング
	assert(SUCCEEDED(result));

	// 値を書き込むと自動的に転送される
	constMapMaterial->color = XMFLOAT4(1, 1, 1, 1);              // RGBA

	float RED = 1;
	float GREEN = 0;
	float BLUE = 0;
	constMapMaterial->color = XMFLOAT4(RED, GREEN, BLUE, 0.5f);

	// ルートパラメータの設定
	D3D12_ROOT_PARAMETER rootParams[3] = {};
	// 定数バッファ0番
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;   // 種類
	rootParams[0].Descriptor.ShaderRegister = 0;                   // 定数バッファ番号
	rootParams[0].Descriptor.RegisterSpace = 0;                    // デフォルト値
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // 全てのシェーダから見える
	// テクスチャレジスタ0番
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;   //種類
	rootParams[1].DescriptorTable.pDescriptorRanges = &descriptorRange;		  //デスクリプタレンジ
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;              		  //デスクリプタレンジ数
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;               //全てのシェーダから見える
	// 定数バッファ1番
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;   // 種類
	rootParams[2].Descriptor.ShaderRegister = 1;                   // 定数バッファ番号
	rootParams[2].Descriptor.RegisterSpace = 0;                    // デフォルト値
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // 全てのシェーダから見える


	// テクスチャサンプラーの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc{};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;                 //横繰り返し（タイリング）
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;                 //縦繰り返し（タイリング）
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;                 //奥行繰り返し（タイリング）
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;  //ボーダーの時は黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                   //全てリニア補間
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;                                 //ミップマップ最大値
	samplerDesc.MinLOD = 0.0f;                                              //ミップマップ最小値
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;           //ピクセルシェーダからのみ使用可能

	ID3DBlob* vsBlob = nullptr; // 頂点シェーダオブジェクト
	ID3DBlob* psBlob = nullptr; // ピクセルシェーダオブジェクト
	ID3DBlob* errorBlob = nullptr; // エラーオブジェクト

	// 頂点シェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/shaders/BasicVS.hlsl", // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);
	// エラーなら
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string error;
		error.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			error.begin());
		error += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(error.c_str());
		assert(0);
	}
	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"BasicPS.hlsl", // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	// エラーなら
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string error;
		error.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			error.begin());
		error += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(error.c_str());
		assert(0);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ // xyz座標(1行で書いたほうが見やすい)
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ //法線ベクトル(1行で書いたほうが見やすい)
		  "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
		  D3D12_APPEND_ALIGNED_ELEMENT,
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0

		},
		{ // uv座標(1行で書いたほうが見やすい)
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};


	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};

	//デプスステンシルステートの設定
	pipelineDesc.DepthStencilState.DepthEnable = true;
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// シェーダーの設定
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();

	// シェーダーの設定
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();

	// サンプルマスクの設定
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	//////////////////////////////////////////////////////////////
	//-------------------ラスタライザの設定------------------------//
	/////////////////////////////////////////////////////////////

	//pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;


	//塗りつぶしかワイヤーのどっちかしか使えない
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴン内塗りつぶし
	//pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // ワイヤーフレーム
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度クリッピングを有効に

	// ブレンドステート
	//pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask
	//	= D3D12_COLOR_WRITE_ENABLE_ALL; // RBGA全てのチャンネルを描画


	//レンダーターゲットのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = pipelineDesc.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //RBGA全てのチャンネルを描画
	//共通設定(アルファ値)
	blenddesc.BlendEnable = true; //ブレンドを有効にする
	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; //加算
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE; //ソースの値を100%使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO; //デストの値を0%使う
	////加算合成
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD; //加算
	//blenddesc.SrcBlend = D3D12_BLEND_ONE; //ソースの値を100%使う
	//blenddesc.DestBlend = D3D12_BLEND_ONE; //デストの値を100%使う
	////減算合成
	//blenddesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT; //デストからソースを減算
	//blenddesc.SrcBlend = D3D12_BLEND_ONE; //ソースの値を100%使う
	//blenddesc.DestBlend = D3D12_BLEND_ONE;              // デストの値を100% 使う
	//// 色反転
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;             // 加算
	//blenddesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;    // 1.0f-デストカラーの値
	//blenddesc.DestBlend = D3D12_BLEND_ZERO;             // 使わない
	// 半透明合成
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD;             // 加算
	blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;         // ソースのアルファ値
	blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;    // 1.0f-ソースのアルファ値

	//頂点レイアウトの設定
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	//図形の形状設定
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// その他の設定
	pipelineDesc.NumRenderTargets = 1; // 描画対象は1つ
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 0~255指定のRGBA
	pipelineDesc.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	// ルートシグネチャ
	ComPtr < ID3D12RootSignature> rootSignature;

	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParams; //ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = _countof(rootParams);        //ルートパラメータ数
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;


	// ルートシグネチャのシリアライズ
	ComPtr < ID3DBlob> rootSigBlob;
	result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob, &errorBlob);
	assert(SUCCEEDED(result));
	result = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(result));

	// パイプラインにルートシグネチャをセット
	pipelineDesc.pRootSignature = rootSignature.Get();

	//πプランステートの生成
	ComPtr < ID3D12PipelineState> pipelineStage;

	result = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineStage));
	assert(SUCCEEDED(result));

#pragma endregion

	float angle = 0.0f; //カメラの回転
	//ゲームループ
	while (true) {
#pragma region ウィンドウメッセージ処理
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//アプリケーションが終わる時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}


#pragma endregion

#pragma region DirectX毎フレーム処理
		/////////////////////////////////////////////////////
		//----------DireceX毎フレーム処理　ここから------------//
		///////////////////////////////////////////////////

		//入力の更新
		input->Update();

		// 数字の0キーが押されていたら
		if (input->PushKey(DIK_0))
		{
			OutputDebugStringA("Hit 0\n");  // 出力ウィンドウに「Hit 0」と表示
		}

		//バックバッファの番号を取得(2つなので0番か1番)
		UINT bbIndex = swapChain->GetCurrentBackBufferIndex();

		//1.リソースバリアで書き込み可能に変更
		D3D12_RESOURCE_BARRIER barrierDesc{};
		barrierDesc.Transition.pResource = backBuffers[bbIndex].Get();
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		commandList->ResourceBarrier(1, &barrierDesc);

		//2.描画先の変更
		//レンダーターゲットビューのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += bbIndex * device->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);
		/*commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);*/
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		//3.画面クリア			R	  G	   B	A
		FLOAT clearColor[] = { 0.1f,0.25f,0.5f,0.0f };//青っぽい色
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		if (input->PushKey(DIK_SPACE))     // スペースキーが押されていたら
		{
			//画面クリアカラーの数値を書き換える
			FLOAT clearColor[] = { 11.1f,0.25f, 0.5f,0.0f }; // ピンクっぽい色
			commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		}


		if (input->PushKey(DIK_D) || input->PushKey(DIK_A)) {
			if (input->PushKey(DIK_D)) { angle += XMConvertToRadians(1.0f); }
			else if (DIK_A) { angle -= XMConvertToRadians(1.0f); }

			//angleラジアンだけy軸まわりに回転
			eye.x = -100 * sinf(angle);
			eye.z = -100 * cosf(angle);
			matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
		}
		if (input->PushKey(DIK_W) || input->PushKey(DIK_S)) {
			if (input->PushKey(DIK_W)) { angle += XMConvertToRadians(1.0f); }
			else if (DIK_S) { angle -= XMConvertToRadians(1.0f); }

			//angleラジアンだけy軸まわりに回転
			eye.y = -100 * sinf(angle);
			eye.z = -100 * cosf(angle);
			matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
		}


		for (size_t i = 0; i < _countof(object3ds); i++)
		{
			UpdateObject3d(&object3ds[i], matView, matProjection);
		}

		//座標操作
		if (input->PushKey(DIK_UP) || input->PushKey(DIK_DOWN) || input->PushKey(DIK_RIGHT) || input->PushKey(DIK_LEFT)) {
			if (input->PushKey(DIK_UP)) { object3ds[0].position.y += 1.0f; }
			else if (input->PushKey(DIK_DOWN)) { object3ds[0].position.y -= 1.0f; }
			if (input->PushKey(DIK_RIGHT)) { object3ds[0].position.x += 1.0f; }
			else if (input->PushKey(DIK_LEFT)) { object3ds[0].position.x -= 1.0f; }
		}

		if (GREEN <= 1.0f) {
			RED -= 0.01f;
			GREEN += 0.01f;
			BLUE += 0.01f;
		}
		constMapMaterial->color = XMFLOAT4(RED, GREEN, BLUE, 0.5f);

		if (input->PushKey(DIK_R)) {
			RED = 1.0f;
			GREEN = 0.0f;
			BLUE = 0.0f;
		}

		////回転
		//if (key[DIK_UP] || key[DIK_DOWN] || key[DIK_RIGHT] || key[DIK_LEFT]) {
		//	if (key[DIK_UP]) { position.z += 1.0f; }
		//	else if (key[DIK_DOWN]) { position.z -= 1.0f; }
		//	if (key[DIK_RIGHT]) { position.x += 1.0f; }
		//	else if (key[DIK_LEFT]) { position.x -= 1.0f; }
		//}

		//matWorld = XMMatrixIdentity();
		//matScale = XMMatrixScaling(scale.x, scale.y, scale.z);
		//matWorld *= matScale;
		////単位行列を代入
		//matRot = XMMatrixIdentity();
		//matRot *= XMMatrixRotationX(XMConvertToRadians(rotation.x));
		//matRot *= XMMatrixRotationY(XMConvertToRadians(rotation.y));
		//matRot *= XMMatrixRotationZ(XMConvertToRadians(rotation.z));
		//matWorld *= matRot;


		//matTrans = XMMatrixTranslation(position.x, position.y, position.z);
		//matWorld *= matTrans;

		////定数バッファに転送
		//constMapTransform0->mat = matWorld * matView * matProjection;



		/////ワールド行列1番の生成///
		////ワールド変換行列
		//XMMATRIX matWorld1;
		//matWorld1 = XMMatrixIdentity();
		////各種変形行列を計算
		//XMMATRIX matScale1 = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		//XMMATRIX matRot1 = XMMatrixRotationY(XM_PI / 4.0f);
		//XMMATRIX matTrans1 = XMMatrixTranslation(-20.0f, 0, 0);
		////ワールド行列を合成
		//matWorld1 = matScale1 * matRot1 * matTrans1;
		////ワールド、ビュー、射影行列を合成してシェーダーに転送
		//constMapTransform1->mat = matWorld1 * matView * matProjection;


		////トリガー処理
		//bool キーを押した状態か(uint8_t キー番号);
		//bool キーを離した状態か(uint8_t キー番号);
		//bool キーを押した瞬間か(uint8_t キー番号);
		//bool キーを離した瞬間か(uint8_t キー番号);

		//// キーボード情報の取得開始
		//keyboard->Acquire();

		//// 全キーの入力状態を取得する

		//keyboard->GetDeviceState(sizeof(key), key);



		//////////////////////////////////////////////
		//-------DireceX毎フレーム処理　ここまで--------//
		////////////////////////////////////////////

#pragma endregion

#pragma region グラフィックスコマンド

		//4.描画コマンドここから

		// ビューポート設定コマンド
		D3D12_VIEWPORT viewport{};
		viewport.Width = WinApp::window_width;   //よこ 最大1280
		viewport.Height = WinApp::window_height;  //たて 最大720
		viewport.TopLeftX = 0;  //左上X
		viewport.TopLeftY = 0;  //左上Y
		viewport.MinDepth = 0.0f; //最小頻度
		viewport.MaxDepth = 1.0f; //最大頻度
		// ビューポート設定コマンドを、コマンドリストに積む
		commandList->RSSetViewports(1, &viewport);

		//シザー矩形
		D3D12_RECT scissorRect{};
		scissorRect.left = 0; // 切り抜き座標左
		scissorRect.right = scissorRect.left + WinApp::window_width; // 切り抜き座標右
		scissorRect.top = 0; // 切り抜き座標上
		scissorRect.bottom = scissorRect.top + WinApp::window_height; // 切り抜き座標下
		// シザー矩形設定コマンドを、コマンドリストに積む
		commandList->RSSetScissorRects(1, &scissorRect);

		//パイプライン巣チートと√シグネチャの設定コマンド
		commandList->SetPipelineState(pipelineStage.Get());
		commandList->SetGraphicsRootSignature(rootSignature.Get());

		// プリミティブ形状の設定コマンド
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 三角形リスト

		//頂点バッファの設定コマンド
		commandList->IASetVertexBuffers(0, 1, &vbView);

		// 定数バッファビュー(CBV)の設定コマンド
		commandList->SetGraphicsRootConstantBufferView(0, constBuffMaterial->GetGPUVirtualAddress());
		// SRVヒープの設定コマンド
		commandList->SetDescriptorHeaps(1, srvHeap.GetAddressOf());
		// SRVヒープの先頭ハンドルを取得（SRVを指しているはず）
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
		// SRVヒープの先頭にあるSRVをルートパラメータ1番に設定
		commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);


		//2枚目を指し示すようにしたSRVのハンドルをルートパラメータ1番に設定
		srvGpuHandle.ptr += incrementSize;
		commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);

		// インデックスバッファビューの設定コマンド
		commandList->IASetIndexBuffer(&ibView);

		//// 0番定数バッファビュー(CBV)の設定コマンド
		//commandList->SetGraphicsRootConstantBufferView(2, constBuffTransform0->GetGPUVirtualAddress());
		//// 描画コマンド
		//commandList->DrawIndexedInstanced(_countof(indices), 1, 0, 0, 0);
		//
		//// 1番定数バッファビュー(CBV)の設定コマンド
		//commandList->SetGraphicsRootConstantBufferView(2, constBuffTransform1->GetGPUVirtualAddress());
		//// 描画コマンド
		//commandList->DrawIndexedInstanced(_countof(indices), 1, 0, 0, 0);

		//全オブジェクトについて処理
		for (int i = 0; i < _countof(object3ds); i++)
		{
			DrawObject3d(&object3ds[i], commandList.Get(), vbView, ibView, _countof(indices));
		}



		//4.描画コマンドここまで

#pragma endregion

#pragma region 画面入れ替え

		//5.リソースバリアを戻す
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;//描画状態から
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;//表示状態へ
		commandList->ResourceBarrier(1, &barrierDesc);

		//命令のクローズ
		result = commandList->Close();
		assert(SUCCEEDED(result));
		//コマンドリストの実行
		ID3D12CommandList* commandLists[] = { commandList.Get()};
		commandQueue->ExecuteCommandLists(1, commandLists);

		//画面に表示するバッファをフリップ(裏表の入れ替え)
		result = swapChain->Present(1, 0);
		assert(SUCCEEDED(result));

		//コマンドの実行完了を待つ
		commandQueue->Signal(fence.Get(), ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			if (event != 0) {
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}
		}

		//キューをクリア
		result = commandAllocator->Reset();
		assert(SUCCEEDED(result));
		//再びコマンドリストを貯める準備
		result = commandList->Reset(commandAllocator.Get(), nullptr);
		assert(SUCCEEDED(result));

#pragma endregion
	}
#pragma region  WindowsAPI後始末

	//WindowsAPIの終了処理
	winApp->Finalize();

	//入力開放
	delete input;
	//WindowsAPI開放
	delete winApp;

#pragma endregion
	return 0;
}
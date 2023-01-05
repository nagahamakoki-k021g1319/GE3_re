#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "FPS.h"
#include "SpriteCommon.h"
#include "Sprite.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

#pragma region WindowsAPI初期化処理

	//ポインタ
	WinApp* winApp = nullptr;
	DirectXCommon* dxCommon = nullptr;
	FPS* fps = new FPS;


	//windowsAPIの初期化
	winApp = new WinApp();
	winApp->Initialize();

	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	MSG msg = {};
	
	//ポインタ
	Input* input = nullptr;

	//入力の初期化　
	input = new Input();
	input->Initialize(winApp);

	SpriteCommon* spriteCommon = nullptr;
	//スプライト共通部分の初期化
	spriteCommon = new SpriteCommon;
	spriteCommon->Initialize(dxCommon);

#pragma endregion

#pragma region DirectX初期化処理

	////////////////////////////////////////////////////
	//-------------DirectX12初期化処理ここから-----------//
	//////////////////////////////////////////////////

	Sprite* sprite = new Sprite();
	sprite->Initialize(spriteCommon);

	XMFLOAT2 position = sprite->GetPosition();
	position.x = 500.0f;
	position.y = 200.0f;
	sprite->SetPozition(position);
	XMFLOAT4 color = { 1,1,1,1 };
	sprite->SetColor(color);
	sprite->SetSize(XMFLOAT2{ 500.0f,281.0f });

	Sprite* sprite2 = new Sprite();
	sprite2->Initialize(spriteCommon);

	XMFLOAT2 position2 = sprite2->GetPosition();
	position.x = 500.0f;
	position.y = 500.0f;
	sprite2->SetPozition(position2);
	XMFLOAT4 color2 = { 1,1,1,1 };
	sprite2->SetColor(color2);
	sprite2->SetSize(XMFLOAT2{ 200.0f,112.0f });
	
	spriteCommon->LoadTexture(0, "eri.png");
	spriteCommon->LoadTexture(1, "feri.png");

	sprite->SetTextureIndex(0);
	sprite2->SetTextureIndex(1);
	

	/////////////////////////////////////////////////////////
	//--------------DirectX12初期化処理　ここまで-------------//
	///////////////////////////////////////////////////////
#pragma endregion

#pragma region 描画初期化処理

	
	//FPS変えたいとき
	fps->SetFrameRate(60);

#pragma endregion

	float angle = 0.0f; //カメラの回転
	//ゲームループ
	while (true) {
#pragma region ウィンドウメッセージ処理
	
		//アプリケーションが終わる時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}
		
		fps->FpsControlBegin();

#pragma endregion

#pragma region DirectX毎フレーム処理
		/////////////////////////////////////////////////////
		//----------DireceX毎フレーム処理　ここから------------//
		///////////////////////////////////////////////////

		//入力の更新
		input->Update();

		//////////////////////////////////////////////
		//-------DireceX毎フレーム処理　ここまで--------//
		////////////////////////////////////////////

#pragma endregion

#pragma region グラフィックスコマンド

		//4.描画コマンドここから
		dxCommon->PreDraw();

		sprite->Draw();
		sprite2->Draw();

		dxCommon->PostDraw();

		fps->FpsControlEnd();
		//4.描画コマンドここまで

#pragma endregion

#pragma region 画面入れ替え

#pragma endregion
	}
#pragma region  WindowsAPI後始末

	//WindowsAPIの終了処理
	winApp->Finalize();

	//入力開放
	delete input;
	//WindowsAPI開放
	delete winApp;
	delete dxCommon;

	delete fps;

#pragma endregion
	return 0;
}
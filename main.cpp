#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "FPS.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "Object3d.h"
#include "Model.h"
#include "Audio.h"
#include "ImGuiManager.h"
#include <imgui.h>


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

	ImGuiManager* ImGuiMan = nullptr;
	ImGuiMan = new ImGuiManager();
	ImGuiMan->Initialize(winApp, dxCommon);

	Audio* audio = nullptr;
	audio = new Audio();
	audio->Initialize();

#pragma endregion

#pragma region DirectX初期化処理
	// 3Dオブジェクト静的初期化
	Object3d::StaticInitialize(dxCommon->GetDevice(), WinApp::window_width, WinApp::window_height);


#pragma endregion

#pragma region 描画初期化処理

	Sprite* sprite = new Sprite();
	sprite->Initialize(spriteCommon);
	XMFLOAT2 position = sprite->GetPosition();
	position.x = 400.0f;
	position.y = 0.0f;
	sprite->SetPozition(position);
	XMFLOAT4 color = { 1,1,1,1 };
	sprite->SetColor(color);
	sprite->SetSize(XMFLOAT2{ 500.0f,281.0f });


	Sprite* sprite1 = new Sprite();
	sprite1->Initialize(spriteCommon);
	XMFLOAT2 position1 = sprite1->GetPosition();
	position1.x = 30.0f;
	position1.y = 30.0f;
	sprite1->SetPozition(position1);
	XMFLOAT4 color1 = { 1,1,1,1 };
	sprite1->SetColor(color1);
	sprite1->SetSize(XMFLOAT2{ 200.0f,112.0f });

	spriteCommon->LoadTexture(0, "eri.png");
	sprite->SetTextureIndex(0);

	spriteCommon->LoadTexture(1, "feri.png");
	sprite1->SetTextureIndex(1);


	// OBJからモデルデータを読み込み
	Model* model = Model::LoadFromOBJ("as");
	Model* model2 = Model::LoadFromOBJ("as2");
	//3Dオブジェクト生成
	Object3d* object3d = Object3d::Create();
	Object3d* object3d_2 = Object3d::Create();
	// オブジェクトにモデルを紐づける
	object3d->SetModel(model);
	object3d_2->SetModel(model2);
	//3Dオブジェクトの位置を指定
	object3d_2->SetPosition({ -5,5,0 });

	

	////////////////////////////
	//------音声読み込み--------//
	///////////////////////////

	audio->LoadWave("cr.wav");
	audio->LoadWave("tit.wav");
	int CheckFlag = 0;


	float f[2] = {100,100};

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

		object3d->Update();
		object3d_2->Update();

		if (CheckFlag == 0) {
			//音声再生
			audio->PlayWave("tit.wav");
			CheckFlag = 1;
		}

		if (input->TriggerKey(DIK_SPACE)) {
			//音声再生
			audio->PlayWave("cr.wav");
		}

		//デバッグテキストはここにはさむ
		ImGuiMan->Bigin();
		
		sprite1->SetPozition({ f[0],f[1] });
		ImGui::SetWindowSize({ 500,100 });
		ImGui::SliderFloat2("ferrisPos", &f[0], 0.0f, 1280.0f, "%.3f");

		//デモウィンドウの表示ON
		ImGui::ShowDemoWindow();

		ImGuiMan->End();


		//////////////////////////////////////////////
		//-------DireceX毎フレーム処理　ここまで--------//
		////////////////////////////////////////////

#pragma endregion

#pragma region グラフィックスコマンド

		//4.描画コマンドここから
		dxCommon->PreDraw();
		//3Dオブジェクト描画前処理
		Object3d::PreDraw(dxCommon->GetCommandList());

		//3Dオブジェクトの描画
		object3d->Draw();
		object3d_2->Draw();


		/// <summary>
		/// ここに3Dオブジェクトの描画処理を追加できる
		/// <summary>

		//3Dオブジェクト描画後処理
		Object3d::PostDraw();

		sprite->Draw();
		sprite1->Draw();


		ImGuiMan->Draw();

		dxCommon->PostDraw();

		fps->FpsControlEnd();
		//4.描画コマンドここまで

#pragma endregion

#pragma region 画面入れ替え

#pragma endregion
	}
#pragma region  WindowsAPI後始末
	ImGuiMan->Finalize();
	//ImGuiの開放
	delete ImGuiMan;

	//3Dオブジェクト解放
	delete object3d;
	delete object3d_2;
	
	//3Dモデル解放
	delete model;
	delete model2;

	//オーディオ解放
	audio->Finalize();
	delete audio;



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
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

	ImGuiManager* imGui = nullptr;
	imGui = new ImGuiManager();
	imGui->Initialize(winApp, dxCommon);

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
	/*position.x = 400.0f;
	position.y = 0.0f;*/
	sprite->SetPozition(position);
	XMFLOAT4 color = { 1,1,1,1 };
	sprite->SetColor(color);
	sprite->SetSize(XMFLOAT2{ 500.0f,281.0f });

	/*Sprite* sprite1 = new Sprite();
	sprite1->Initialize(spriteCommon);
	XMFLOAT2 position1 = sprite1->GetPosition();
	position1.x = 30.0f;
	position1.y = 30.0f;
	sprite1->SetPozition(position1);
	XMFLOAT4 color1 = { 1,1,1,1 };
	sprite1->SetColor(color1);
	sprite1->SetSize(XMFLOAT2{ 200.0f,112.0f });*/

	Sprite* sprite2 = new Sprite();
	sprite2->Initialize(spriteCommon);
	XMFLOAT2 position2 = sprite2->GetPosition();
	sprite2->SetPozition(position2);
	sprite2->SetSize(XMFLOAT2{ 1280.0f,720.0f });

	

	spriteCommon->LoadTexture(0, "eri.png");
	sprite->SetTextureIndex(0);

	


	/*spriteCommon->LoadTexture(1, "feri.png");
	sprite1->SetTextureIndex(1);*/

	//sprite->SetPozition({ 0,0 });

	spriteCommon->LoadTexture(2, "tt.png");
	sprite2->SetTextureIndex(2);


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
	object3d_2->SetPosition({ -5,-10,0 });

	////////////////////////////
	//------音声読み込み--------//
	///////////////////////////

	audio->LoadWave("cr.wav");
	audio->LoadWave("tit.wav");
	int CheckFlag = 0;

	int isCollision = 0;

	IXAudio2SourceVoice* pSourceVoice[10] = { 0 };

	float f[2] = {100,100};

	//シーン切り替え
	enum class SceneNo {
		Title, //タイトル
		Game,  //射撃
	};

	SceneNo sceneNo_ = SceneNo::Title;


	//FPS変えたいとき
	fps->SetFrameRate(60);

#pragma endregion

	//ゲームループ
	while (true) {
#pragma region ウィンドウメッセージ処理
	
		//window
		if (winApp->ProcessMessage()) {
			//抜ける
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

		if (sceneNo_ == SceneNo::Title) {
			if (input->TriggerKey(DIK_SPACE)) {
				sceneNo_ = SceneNo::Game;
				audio->PlayWave("cr.wav");
			}
		}


		if (sceneNo_ == SceneNo::Game) {
			object3d->Update();
			// オブジェクト移動
			if (input->PushKey(DIK_UP) || input->PushKey(DIK_W) ||
				input->PushKey(DIK_DOWN) || input->PushKey(DIK_S) ||
				input->PushKey(DIK_RIGHT) || input->PushKey(DIK_D) ||
				input->PushKey(DIK_LEFT) || input->PushKey(DIK_A))
			{
				// 現在の座標を取得
				XMFLOAT3 position = object3d->GetPosition();

				// 移動後の座標を計算
				if (input->PushKey(DIK_UP) || input->PushKey(DIK_W)) { position.y += 0.3f; }
				else if (input->PushKey(DIK_DOWN) || input->PushKey(DIK_S)) { position.y -= 0.3f; }
				if (input->PushKey(DIK_RIGHT) || input->PushKey(DIK_D)) { position.x += 0.3f; }
				else if (input->PushKey(DIK_LEFT) || input->PushKey(DIK_A)) { position.x -= 0.3f; }

				if (input->PushKey(DIK_UP) && input->PushKey(DIK_SPACE) || input->PushKey(DIK_W) && input->PushKey(DIK_SPACE)) { position.y += 0.6f; }
				else if (input->PushKey(DIK_DOWN) && input->PushKey(DIK_SPACE) || input->PushKey(DIK_S) && input->PushKey(DIK_SPACE)) { position.y -= 0.6f; }
				if (input->PushKey(DIK_RIGHT) && input->PushKey(DIK_SPACE) || input->PushKey(DIK_D) && input->PushKey(DIK_SPACE)) { position.x += 0.6f; }
				else if (input->PushKey(DIK_LEFT) && input->PushKey(DIK_SPACE) || input->PushKey(DIK_A) && input->PushKey(DIK_SPACE)) { position.x -= 0.6f; }
				// 座標の変更を反映
				object3d->SetPosition(position);
			}



			object3d_2->Update();

			//当たり判定
			XMFLOAT3 a = object3d_2->GetPosition();
			XMFLOAT3 b = object3d->GetPosition();
			float xyz = std::pow(a.x - b.x, 2.0f) + std::pow(a.y - b.y, 2.0f) + std::pow(a.z - b.z, 2.0f);
			float lenR = std::pow(1.0f + 1.0f, 2.0f);
			if (xyz <= lenR) {
				isCollision = 1;
			}
			position.x += 1.0f;
			sprite->SetPozition(position);
	

			if (CheckFlag == 0) {
				//音声再生
				pSourceVoice[0] = audio->PlayWave("tit.wav");
				CheckFlag = 1;
			}
			if (input->TriggerKey(DIK_E)) {
				pSourceVoice[0]->Stop();
			}
			if (input->TriggerKey(DIK_Z)) {
				pSourceVoice[0]->SetVolume(0.1f);
			}if (input->TriggerKey(DIK_X)) {
				pSourceVoice[0]->SetVolume(1.0f);
			}

			if (input->TriggerKey(DIK_SPACE)) {
				//音声再生
				audio->PlayWave("cr.wav");
			}

			//デバッグテキストはここにはさむ
			imGui->Bigin();

			/*sprite1->SetPozition({ f[0],f[1] });*/
			ImGui::SetWindowSize({ 500,100 });
			ImGui::SliderFloat2("ferrisPos", &f[0], 0.0f, 1280.0f, "%.3f");

			//デモウィンドウの表示ON
			ImGui::ShowDemoWindow();

			imGui->End();

		}

		//////////////////////////////////////////////
		//-------DireceX毎フレーム処理　ここまで--------//
		////////////////////////////////////////////

#pragma endregion

#pragma region グラフィックスコマンド

		//4.描画コマンドここから
		dxCommon->PreDraw();
		//3Dオブジェクト描画前処理
		Object3d::PreDraw(dxCommon->GetCommandList());

		if (sceneNo_ == SceneNo::Game) {
			//3Dオブジェクトの描画
			object3d->Draw();
			if (isCollision == 0) {
				object3d_2->Draw();
			}else{}
		}

		/// <summary>
		/// ここに3Dオブジェクトの描画処理を追加できる
		/// <summary>

		//3Dオブジェクト描画後処理
		Object3d::PostDraw();

		if (sceneNo_ == SceneNo::Title) {
			sprite2->Draw();
		}

		if (sceneNo_ == SceneNo::Game) {
			sprite->Update();
			sprite->Draw();
			/*sprite1->Draw();*/


			imGui->Draw();
		}

		dxCommon->PostDraw();

		fps->FpsControlEnd();
		//4.描画コマンドここまで

#pragma endregion

#pragma region 画面入れ替え

#pragma endregion
	}
#pragma region  WindowsAPI後始末
	imGui->Finalize();
	//ImGuiの開放
	delete imGui;

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
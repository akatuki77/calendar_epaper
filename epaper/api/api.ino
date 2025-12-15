// 疎通確認
#include <M5Unified.h>   // M5Stackデバイス(M5Paper S3含む)を統合的に扱うための基本ライブラリ
#include <WiFi.h>        // WiFi接続機能を使うために必要
#include <HTTPClient.h>  // HTTPリクエスト（GETやPOST）を送信するために必要
#include <ArduinoJson.h> // HTTPの応答(JSON形式)を解析するために必要
#include "time.h"        // 時刻取得をするために必要
#include "secrets.h"     // WiFi設定情報ファイル（ssid、password）を使うために必要

// 接続するWiFiネットワークのSSID（名前）
const char* ssid = SECRET_SSID;     // secrets.h で定義した変数

// WiFiのパスワード
const char* password = SECRET_PASS; // secrets.h で定義した変数

// アクセスする「JSONを返すAPI」のURL（Nginx経由でFastAPIの /api/imageName エンドポイントを指す）
const char* calender_url = "http://10.200.1.58:8080/api/dashboard";
const char* week_url = "http://10.200.1.58:8080/api/weekData";

// 最初に表示される画像URL
const char* current_url = calender_url;

// モード管理 (false: Dashboard, true: Week)
bool weekMode = false;

// 時計用グローバル変数
const int TIME_X = 840; // 時計を描画するX座標 (例: 右寄せ)
const int TIME_Y = 20; // 時計を描画するY座標 (例: 上寄せ)
const int TIME_W = 200; // 時計領域の幅
const int TIME_H = 30;  // 時計領域の高さ
char timeStringBuff[20]; // 時刻用 (例: "15:30:05")
int lastMinute = -1; // 前回表示した「分」（初期値はありえない値にする）
struct tm timeinfo; // 時刻情報を格納する構造体

// 更新管理
int cnt = 0; 
String lastETag = ""; 

// サーバーを確認し、更新があれば描画まで行う関数
void checkAndUpdateCalendar() {
  // WiFiがつながっていなければ何もしない
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("[Check] Checking URL: ");
  Serial.println(current_url); // 現在のURLを表示

  HTTPClient http;
  
  // ここに対象のURL変数を指定
  http.begin(current_url); 

  // --- ETagの照合リクエスト設定 ---
  // もし前回のETagを持っていれば、サーバーに「これと同じですか？」と聞く
  if (lastETag.length() > 0) {
    http.addHeader("If-None-Match", lastETag);
  }

  // --- レスポンスヘッダーの取得設定 ---
  // "Content-Type" (画像かどうか) と "ETag" (指紋) を取得できるようにする
  const char* headerKeys[] = {"Content-Type", "ETag"};
  http.collectHeaders(headerKeys, 2);

  // --- リクエスト送信 ---
  int httpCode = http.GET();

  // --- 結果による分岐 ---
  if (httpCode == HTTP_CODE_OK) { // 200 OK: 更新あり (または初回)
    Serial.println("[Update] 200 OK - New content received.");

    // 新しいETagが来ていれば保存する
    if (http.hasHeader("ETag")) {
      lastETag = http.header("ETag");
    }

    // 画像データかどうかのチェック
    String contentType = http.header("Content-Type");

    if (contentType.indexOf("image") >= 0) {
      Serial.println("[Update] Drawing image...");
      
      // 画像データを取得
      String payload = http.getString();
      
      if (payload.length() > 0) {
        // 画像を描画するために「高画質モード」へ
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        bool drawn = false;
        
        // JPEGかPNGかで描画関数を使い分ける
        if (contentType.indexOf("png") >= 0) {
            drawn = M5.Display.drawPng((const uint8_t*)payload.c_str(), payload.length(), 0, 0);
        } else {
            drawn = M5.Display.drawJpg((const uint8_t*)payload.c_str(), payload.length(), 0, 0);
        }

        if (drawn) {
          // ボタンを描画
          drawModeButton();

          M5.Display.display(); // 画面更新(暗転)
        } else {
          Serial.println("[Update] Draw failed.");
        }

        // 時計表示のために「高速モード」に戻す
        M5.Display.setEpdMode(epd_mode_t::epd_fast);
        
        // 画像更新で時計が消えたので、すぐに再描画するようフラグをリセット
        lastMinute = -1; 
      }
    } else {
        Serial.println("[Update] Received non-image data. Ignoring.");
    }

  } else if (httpCode == 304) { // 304 Not Modified: 更新なし
    // サーバーが「データは変わってないよ」と返してきた場合
    Serial.println("[Check] 304 Not Modified.");
    // 更新がないので、何もしない

  } else {
    // その他のエラー
    Serial.printf("[Check] HTTP Failed: %d\n", httpCode);
  }

  http.end();
}

// ボタン描画関数
void drawModeButton() {
  // 枠線を描く
  M5.Display.drawRect(0, 0, 80, 80, TFT_BLACK);

  // フォント設定 (日本語フォント efontJA_24 を使用)
  M5.Display.setFont(&fonts::efontJA_24);
  M5.Display.setTextSize(1); // フォント自体が大きいので倍率は1でOK
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE); // 黒文字、背景白

  // モードによって文字を変える
  if (weekMode) {
    // 週表示のとき
    M5.Display.drawString("月へ", 15, 30); // 座標は枠の中央あたりに調整
  } else {
    // 月表示のとき (ダッシュボード)
    M5.Display.drawString("週へ", 15, 30);
  }
}

void setup() {
  // --- M5Paper S3本体の初期化 ---
  // M5Paper S3用の設定をロード
  auto cfg = M5.config();
  
  // M5Paper S3本体の初期化（この中でM5.Displayなども初期化される）
  M5.begin(cfg);
  
  // PCとUSBで接続し、シリアルモニタにデバッグログ（"Connecting..."など）を表示させるための設定
  Serial.begin(115200);

  // --- ディスプレイ（電子ペーパー）の設定 ---
  // 画面の向きを 1 (横向き、USBが右) に設定
  // 0=縦, 1=横(USB右), 2=逆さ縦, 3=横(USB左)
  M5.Display.setRotation(1); 
  
  // EPD(電子ペーパー)の描画モードを「高速(epd_fast)」に設定
  M5.Display.setEpdMode(epd_mode_t::epd_quality); 
  
  drawModeButton();
  
  // WiFiに接続を開始
  WiFi.begin(ssid, password);
  
  // WiFi.status()が WL_CONNECTED (接続完了) になるまで、ループして待つ
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // 0.5秒待つ
  }     

  // 初回更新
  checkAndUpdateCalendar();
  
  // 描画した画像を、ここで「同時に」EPDに反映
  M5.Display.display(); 

  // --- NTP（時計）のセットアップ ---
  const long  gmtOffset_sec = 9 * 3600; // 日本のタイムゾーン (JST: 9時間 * 3600秒) を設定
  const int   daylightOffset_sec = 0;   // 夏時間はなし
  
  // NTPサーバー（インターネット上の時計サーバー）を設定
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.google.com");

  // これ以降の描画は「部分更新 (epd_fast)」モードで行うよう、モードを切り替える（loop()での display() は「暗転」しなくなる。）
  M5.Display.setEpdMode(epd_mode_t::epd_fast); 
}

// タッチ判定関数
void handleTouch() {
  // タッチされているか？
  if (M5.Touch.getCount() > 0) {
    auto detail = M5.Touch.getDetail(0);
    
    // 指が離れた瞬間に判定
    if (detail.wasClicked()) {
      Serial.printf("Touched at X:%d, Y:%d\n", detail.x, detail.y);

      // 左上 (0,0) から 幅80, 高さ80 の範囲内か
      if (detail.x >= 0 && detail.x < 80 && 
          detail.y >= 0 && detail.y < 80) {
        
        Serial.println(">>> Top-Left Button Clicked! Switching...");

        // モード反転
        weekMode = !weekMode;

        // URLの切り替え
        if (weekMode) {
          current_url = week_url;
        } else {
          current_url = calender_url;
        }

        // 強制更新のためにETagを捨てる
        lastETag = "";

        // 更新実行
        checkAndUpdateCalendar();
        
        // カウンターリセット
        cnt = 0; 
      }
    }
  }
}

// 時間更新関数
void timeChange() {
  // 時刻が取得できた場合のみ処理
  if (getLocalTime(&timeinfo)) {
    // 「今の分」と「最後に表示した分」が違うなら更新する（起動直後は -1 と 33（33分）で違うので即実行される → 次は 34（34分） になった瞬間に実行される）
    if (timeinfo.tm_min != lastMinute) {
      // 画像更新用変数
      cnt++;

      if (cnt >= 5) {
        // 画像更新関数を呼び出す
        checkAndUpdateCalendar();
        cnt = 0; // リセット
      }

      // 最後に表示した分を更新
      lastMinute = timeinfo.tm_min;

      // 時刻用の文字列をフォーマット (秒数なし"%H:%M") (秒数あり"%H:%M") 
      strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);

      // 古い時刻を消す
      // 時計領域(TIME_W, TIME_H)だけを「白」で塗りつぶす
      M5.Display.fillRect(TIME_X, TIME_Y, TIME_W, TIME_H, TFT_WHITE);
      
      // フォントの「形」を Font2 (セリフ体) に設定
      M5.Display.setFont(&fonts::Font4);

      // フォントサイズを設定
      M5.Display.setTextSize(1.5); 
      
      // 時刻を描画
      M5.Display.drawString(timeStringBuff, TIME_X, TIME_Y);

      // 時計領域「だけ」をEPDに反映（ここが「部分更新」のコードで、画像は更新されない）
      M5.Display.display(TIME_X, TIME_Y, TIME_W, TIME_H);
    } 

  } else {
      Serial.println("Failed to get time in loop");
    }
}

void loop() {
  M5.update();   // 必須
  handleTouch(); // タッチ判定
  timeChange();  // 時計更新
  delay(10); 
}
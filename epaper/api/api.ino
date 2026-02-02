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
const char* calender_url = "http://10.200.2.119:8080/api/dashboard";
const char* week_url = "http://10.200.2.119:8080/api/weekData";

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

// --- カレンダー年月管理用 ---
int displayYear = 0;  // 現在表示している年
int displayMonth = 0; // 現在表示している月
String dynamicUrlBuffer = ""; // 作成したURLを保持しておくバッファ

// サーバーを確認し、更新があれば描画まで行う関数
void checkAndUpdateCalendar() {
  // WiFiがつながっていなければ何もしない
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("[Check] Checking URL: ");
  Serial.println(current_url); // 現在のURLを表示

  HTTPClient http;
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
    Serial.println("[Update] 200 OK");

    // 新しいETagが来ていれば保存する
    if (http.hasHeader("ETag")) {
      lastETag = http.header("ETag");
    }

    // 画像データかどうかのチェック
    String contentType = http.header("Content-Type");

    if (contentType.indexOf("image") >= 0) {
      // 画像データを取得
      String payload = http.getString();
      
      if (payload.length() > 0) {
        Serial.println("[Update] Drawing...");
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
        } 

        // 時計表示のために「高速モード」に戻す
        M5.Display.setEpdMode(epd_mode_t::epd_fast);
        
        // 画像更新で時計が消えたので、すぐに再描画するようフラグをリセット
        lastMinute = -1; 
      }
    } 
  } else if (httpCode == 304) { // 304 Not Modified: 更新なし
    // サーバーが「データは変わってないよ」と返してきた場合
    Serial.println("[Check] 304 Not Modified.");
  } 
  http.end();
}

// 年月パラメータ付きURLを生成して更新を実行する関数
void updateMonthParams() {
  // ベースURL + 年 + 月 を結合
  dynamicUrlBuffer = String(calender_url) + 
                     "?year=" + String(displayYear) + 
                     "&month=" + String(displayMonth);
  
  // 生成したURLを現在のターゲットに設定
  current_url = dynamicUrlBuffer.c_str();

  // 強制更新を実行
  lastETag = ""; // ETagをリセットして必ずダウンロードさせる
  checkAndUpdateCalendar();
  cnt = 0; // 自動更新タイマーリセット
}

// ボタン描画関数
void drawModeButton() {
  // 枠線を描く
  M5.Display.drawRect(10, 10, 70, 70, TFT_BLACK);

  // フォント設定 (日本語フォント efontJA_24 を使用)
  M5.Display.setFont(&fonts::efontJA_24);
  M5.Display.setTextSize(1); // フォント自体が大きいので倍率は1でOK
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE); // 黒文字、背景白

  // モードによって文字を変える
  if (weekMode) {
    // 週表示のとき
    M5.Display.drawString("月へ", 25, 35); // 座標は枠の中央あたりに調整
  } else {
    // 月表示のとき (ダッシュボード)
    M5.Display.drawString("週へ", 25, 35);

    // ◁ (左向きの三角枠)
    // 頂点3つの座標 (x1, y1), (x2, y2), (x3, y3), 色 を指定します
    M5.Display.drawTriangle(
        190, 40,  // 左の頂点 (尖っている部分)
        230, 20,  // 右上の頂点
        230, 60,  // 右下の頂点
        TFT_BLACK
    );

    // ▷ (右向きの三角枠)
    M5.Display.drawTriangle(
        480, 40, // 右の頂点 (尖っている部分)
        440, 20,  // 左上の頂点
        440, 60,  // 左下の頂点
        TFT_BLACK
    );

    // 今月へボタン
    M5.Display.drawRect(580, 10, 80, 70, TFT_BLACK);
    M5.Display.drawString("今月へ", 585, 30);
  }
}

// タッチ判定関数
void handleTouch() {
  // タッチされているか？
  if (M5.Touch.getCount() > 0) {
    auto detail = M5.Touch.getDetail(0);
    
    // 指が離れた瞬間に判定
    if (detail.wasClicked()) {
      // 「週へ/月へ」ボタン
      if (detail.x >= 0 && detail.x < 80 && detail.y >= 0 && detail.y < 80) {
        weekMode = !weekMode; // モード反転

        if (weekMode) {
          // 週表示へ切り替え
          current_url = week_url;
          lastETag = "";
          checkAndUpdateCalendar();
          cnt = 0;
        } else {
          // 月表示へ切り替え
          updateMonthParams();         
        }
      } else if (!weekMode && detail.x >= 570 && detail.x < 650 && detail.y >= 0 && detail.y < 80) {
        // 「今月へ」ボタン
        if (getLocalTime(&timeinfo)) {
          displayYear = timeinfo.tm_year + 1900;
          displayMonth = timeinfo.tm_mon + 1;
          updateMonthParams(); // ★ここも重複コードを削除して関数呼び出しに
        }
      } else if (!weekMode && detail.x >= 190 && detail.x < 250 && detail.y >= 10 && detail.y < 70) {
        // 「◁」ボタン (前の月へ)
        displayMonth--;      // 月を減らす
        if (displayMonth < 1) { 
          displayMonth = 12; // 1月より前なら12月へ
          displayYear--;     // 年を減らす
        }
        
        updateMonthParams(); // 更新実行
      } else if (!weekMode && detail.x >= 440 && detail.x < 500 && detail.y >= 10 && detail.y < 70) {
        // 「▷」ボタン (次の月へ)
        displayMonth++;      // 月を増やす
        if (displayMonth > 12) { 
          displayMonth = 1;  // 12月より後なら1月へ
          displayYear++;     // 年を増やす
        }

        updateMonthParams(); // 更新実行
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

      if (cnt >= 30) { // 30分ごとに自動更新チェック
        checkAndUpdateCalendar(); // 画像更新関数を呼び出す
        cnt = 0; // リセット
      }

      lastMinute = timeinfo.tm_min; // 最後に表示した分を更新
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
  M5.Display.clear(TFT_WHITE); // 画面全体を白でクリア
  
  drawModeButton();
  M5.Display.display(); // 画面更新(暗転)
  
  // WiFiに接続を開始
  WiFi.begin(ssid, password);
  
  // WiFi.status()が WL_CONNECTED (接続完了) になるまで、ループして待つ
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // 0.5秒待つ
  }     

  // NTP設定
  configTime(9 * 3600, 0, "pool.ntp.org", "time.google.com");

  if (getLocalTime(&timeinfo)) {
    // 現在の年月を初期値にセット
    displayYear = timeinfo.tm_year + 1900;
    displayMonth = timeinfo.tm_mon + 1;
  } else {
    // 取得失敗時のデフォルト値
    displayYear = 2025; 
    displayMonth = 1;
  }

  updateMonthParams(); // 初回カレンダー表示

  // これ以降の描画は「部分更新 (epd_fast)」モードで行うよう、モードを切り替える（loop()での display() は「暗転」しなくなる。）
  M5.Display.setEpdMode(epd_mode_t::epd_fast); 
}

void loop() {
  M5.update();   // 必須
  handleTouch(); // タッチ判定
  timeChange();  // 時計更新
  delay(10); 
}
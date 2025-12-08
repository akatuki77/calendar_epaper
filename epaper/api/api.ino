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
const char* calender_url = "http://10.200.0.229:8080/api/dashboard";

// 時計用グローバル変数
const int TIME_X = 840; // 時計を描画するX座標 (例: 右寄せ)
const int TIME_Y = 20; // 時計を描画するY座標 (例: 上寄せ)
const int TIME_W = 200; // 時計領域の幅
const int TIME_H = 30;  // 時計領域の高さ
char timeStringBuff[20]; // 時刻用 (例: "15:30:05")
int lastMinute = -1; // 前回表示した「分」（初期値はありえない値にする）
struct tm timeinfo; // 時刻情報を格納する構造体

int cnt = 0; //　画像更新時間間隔用

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
  
  // 画面全体を白(TFT_WHITE)で塗りつぶす（クリアする）
  M5.Display.fillScreen(TFT_WHITE);
  
  // これ以降に描画する文字のサイズを 2 (標準の2倍) に設定
  M5.Display.setTextSize(2);        
  
  // これ以降に描画する文字の色を 黒(TFT_BLACK) に設定
  M5.Display.setTextColor(TFT_BLACK);
  
  // WiFiに接続を開始
  WiFi.begin(ssid, password);
  
  // WiFi.status()が WL_CONNECTED (接続完了) になるまで、ループして待つ
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // 0.5秒待つ
  }     

  connectionHTTP(); // JSON APIへのリクエスト
  
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

// // 指定されたURLが画像(image/...)かどうかを確認する関数
// bool isUrlImage(const char* url) {
//   if (WiFi.status() != WL_CONNECTED) return false;

//   HTTPClient http;
//   Serial.print("[Check] Checking Content-Type: ");
//   Serial.println(url);

//   http.begin(url);
  
//   // ヘッダー情報のうち "Content-Type" を取得するように設定
//   const char * headerKeys[] = {"Content-Type"};
//   http.collectHeaders(headerKeys, 1);

//   // HEADリクエストを送信 (データ本体は取得せず、ヘッダーだけ取得)
//   // ※サーバーによってはHEADを禁止している場合があります。その場合は "GET" に変えてください。
//   int httpCode = http.sendRequest("HEAD"); 

//   bool isImage = false;

//   if (httpCode == HTTP_CODE_OK) {
//     String contentType = http.header("Content-Type");
//     Serial.println("[Check] Type: " + contentType);

//     // "image" という文字が含まれているか確認 (例: "image/jpeg", "image/png")
//     if (contentType.indexOf("image") >= 0) {
//       isImage = true;
//     }
//   } else {
//     Serial.printf("[Check] Failed or not 200 OK. Code: %d\n", httpCode);
//   }

//   http.end();
//   return isImage;
// }

void loop() {
  timeChange();
  delay(10); // ループがビジーになるのを防ぐ
}

void connectionHTTP() {
  // HTTPリクエストを実行するための「HTTPClient」オブジェクト（道具）を準備
  HTTPClient http;
  
  // HTTP道具に、目標のURL(calender_url)をセット
  http.begin(calender_url); 
  
  // 10秒待っても応答がなければタイムアウトとして接続を切る設定
  http.setTimeout(10000); 

  // HTTP GETリクエストを送信（結果として、HTTPステータスコードが httpCode に返ってくる）
  int httpCode = http.GET();

  // --- JSON APIの応答を解析 ---
  // httpCode が HTTP_CODE_OK (== 200、つまり「成功」) だった場合
  if (httpCode == HTTP_CODE_OK) {
    // サーバーから返ってきたデータ(画像バイト)を全て String バッファに読み込む
    String payload = http.getString();

    if (payload.length() > 0) {
      Serial.printf("Data received: %d bytes. Drawing...\n", payload.length());

      // 画質モードをここで変更 (綺麗に表示するため)
      M5.Display.setEpdMode(epd_mode_t::epd_quality);
      
      // 読み込んだデータを drawJpg に渡す
      // python側が JPEG なら drawJpg、PNG なら drawPng を使う
      bool success = M5.Display.drawPng(
        (const uint8_t*)payload.c_str(), // データの先頭ポインタ
        payload.length(),                // データのサイズ
        0,                               // X座標
        0                                // Y座標
      );

      if (!success) {
        Serial.println("Draw failed! (Format error?)");
        M5.Display.drawString("Draw Failed", 10, 50);
      }

      // 画面に反映 (暗転リフレッシュ)
      M5.Display.display();

    } else {
      Serial.println("Payload is empty.");
    }
  } else {
    Serial.printf("HTTP Failed. Code: %d\n", httpCode);
    M5.Display.drawString("HTTP Error", 10, 50);
    M5.Display.display();
  }

  http.end();

  M5.Display.setEpdMode(epd_mode_t::epd_fast); 
}

void timeChange() {
  // 時刻が取得できた場合のみ処理
  if (getLocalTime(&timeinfo)) {
    // 「今の分」と「最後に表示した分」が違うなら更新する（起動直後は -1 と 33（33分）で違うので即実行される → 次は 34（34分） になった瞬間に実行される）
    if (timeinfo.tm_min != lastMinute) {
      // 画像更新用変数
      cnt++;

      if (cnt >= 1) {
        // 画像更新関数を呼び出す
        connectionHTTP();

        // // URLの中身が画像であるかチェックし、画像の場合のみ更新する
        // if (isUrlImage(calender_url)) {
        //     Serial.println("[Update] It's an image. Updating...");
            
        //     // 既存の画像更新関数
        //     connectionHTTP();
        // } else {
        //     Serial.println("[Update] Not an image. Skip update.");
        // }

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
// 疎通確認
#include <M5Unified.h>   // M5Stackデバイス(M5Paper S3含む)を統合的に扱うための基本ライブラリ
#include <WiFi.h>        // WiFi接続機能を使うために必要
#include <HTTPClient.h>  // HTTPリクエスト（GETやPOST）を送信するために必要
#include <ArduinoJson.h> // HTTPの応答(JSON形式)を解析するために必要
#include "time.h"       // 時刻取得のために、この行を追加
#include "secrets.h"

// 接続するWiFiネットワークのSSID（名前）
const char* ssid = SECRET_SSID;     // secrets.h で定義した変数

// WiFiのパスワード
const char* password = SECRET_PASS; // secrets.h で定義した変数

// ステップ1でアクセスする「JSONを返すAPI」のURL
// (Nginx経由でFastAPIの /image エンドポイントを指す)
const char* nginx_url = "http://10.200.0.224:8080/api/data"; 

// setup()関数は、M5Paperが起動したときに「一度だけ」実行される
void setup() {
  // --- 1. M5Paper S3本体の初期化 ---
  
  // M5Paper S3用の設定をロード
  auto cfg = M5.config();
  
  // M5Paper S3本体の初期化（この中でM5.Displayなども初期化されます）
  M5.begin(cfg);
  
  // PCとUSBで接続し、シリアルモニタにデバッグログ（"Connecting..."など）を表示させるための設定
  Serial.begin(115200);

  // --- 2. ディスプレイ（電子ペーパー）の設定 ---
  
  // 画面の向きを 1 (横向き、USBが右) に設定
  // 0=縦, 1=横(USB右), 2=逆さ縦, 3=横(USB左)
  M5.Display.setRotation(1); 
  
  // EPD(電子ペーパー)の描画モードを「高速(epd_fast)」に設定
  // M5.Display.setEpdMode(epd_mode_t::epd_fast); 
  M5.Display.setEpdMode(epd_mode_t::epd_quality); 
  
  // 画面全体を白(TFT_WHITE)で塗りつぶす（クリアする）
  M5.Display.fillScreen(TFT_WHITE);
  
  // これ以降に描画する文字のサイズを 2 (標準の2倍) に設定
  M5.Display.setTextSize(2);        
  
  // これ以降に描画する文字の色を 黒(TFT_BLACK) に設定
  M5.Display.setTextColor(TFT_BLACK);

  // --- 3. WiFiへの接続 ---
  
  // WiFiに接続を開始
  WiFi.begin(ssid, password);
  
  // WiFi.status()が WL_CONNECTED (接続完了) になるまで、ループして待つ
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // 0.5秒待つ
    // Serial.print("."); // 待っている間、"." を表示し続ける
  }     

  // --- 4. ステップ1: JSON APIへのリクエスト ---

  // HTTPリクエストを実行するための「HTTPClient」オブジェクト（道具）を準備
  HTTPClient http;
  
  // HTTP道具に、目標のURL(nginx_url)をセット
  http.begin(nginx_url); 
  
  // 10秒待っても応答がなければタイムアウトとして接続を切る設定
  http.setTimeout(10000); 

  // HTTP GETリクエストを「送信！」
  // 結果として、HTTPステータスコード（200, 404, -1 など）が httpCode に返ってくる
  int httpCode = http.GET();

  // --- 5. ステップ2: JSON APIの応答を解析 ---
  
  // httpCode が HTTP_CODE_OK (== 200、つまり「成功」) だった場合
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Download success. Reading data...");

    // 1. サーバーから返ってきたデータ(画像バイト)を全て String バッファに読み込む
    //    (M5Paper S3 はメモリが大きいので、これが一番安定します)
    String payload = http.getString();

    if (payload.length() > 0) {
      Serial.printf("Data received: %d bytes. Drawing...\n", payload.length());

      // 画質モードをここで変更 (綺麗に表示するため)
      M5.Display.setEpdMode(epd_mode_t::epd_quality);
      
      // 2. 読み込んだデータを drawJpg に渡す
      //    python側が JPEG なら drawJpg、PNG なら drawPng を使うこと！
      bool success = M5.Display.drawPng(
        (const uint8_t*)payload.c_str(), // データの先頭ポインタ
        payload.length(),                // データのサイズ
        0,                               // X座標
        0                                // Y座標
      );

      if (success) {
        Serial.println("Draw success!");
      } else {
        Serial.println("Draw failed! (Format error?)");
        M5.Display.drawString("Draw Failed", 10, 50);
      }

      // 3. 画面に反映 (暗転リフレッシュ)
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



  // --- 8. 最終ステップ: 画面に「一度だけ」反映 ---
  
  // 描画した1枚目と2枚目の画像を、ここで「同時に」EPDに反映
  M5.Display.display(); 

  // --- 9. NTP（時計）のセットアップ ---
  
  // 日本のタイムゾーン (JST: 9時間 * 3600秒) を設定
  const long  gmtOffset_sec = 9 * 3600; 
  const int   daylightOffset_sec = 0; // 夏時間はなし
  
  // NTPサーバー（インターネット上の時計サーバー）を設定
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.google.com");

  struct tm timeinfo; // 時刻情報を格納する構造体

  // --- 10.最重要 ---
  // これ以降の描画は「部分更新 (epd_fast)」モードで行うよう、モードを切り替える
  // これにより、loop()での display() は「暗転」しなくなります。
  M5.Display.setEpdMode(epd_mode_t::epd_fast); 

  M5.Display.display(); 
}



// --- グローバル変数 (時計用) ---
unsigned long lastTimeUpdate = 0; // 最後に時刻を更新した時間
const int TIME_X = 720; // 時計を描画するX座標 (例: 右寄せ)
const int TIME_Y = 480; // 時計を描画するY座標 (例: 右寄せ)
const int TIME_W = 200; // 時計領域の幅 (フォントサイズ3で 10文字 "YYYY/MM/DD" が入る幅)
const int TIME_H = 80;  // 時計領域の高さ (フォントサイズ3の2行分)

// 文字列バッファ (日付と時刻の2つに増やす)
char dateStringBuff[20]; // 日付用 (例: "2025/10/30")
char timeStringBuff[20]; // 時刻用 (例: "15:30:05")



// loop()関数は、setup()が完了した後に「無限に」呼び出され続けます
void loop() {
  // 1秒ごと (1000ミリ秒) に時刻を更新
  if (millis() - lastTimeUpdate > 1000) {
    
    lastTimeUpdate = millis(); // 最終更新時刻を更新

    struct tm timeinfo; // 時刻情報を格納する構造体
    
    if (getLocalTime(&timeinfo)) { // NTPから現在時刻を取得
      
      // 1. 日付用の文字列をフォーマット ("YYYY/MM/DD")
      strftime(dateStringBuff, sizeof(dateStringBuff), "%Y/%m/%d", &timeinfo);
      
      // 2. 時刻用の文字列をフォーマット ("HH:MM:SS")
      strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);

      // 1. 古い時刻を消す
      // 時計領域(TIME_W, TIME_H)だけを「白」で塗りつぶす
      M5.Display.fillRect(TIME_X, TIME_Y, TIME_W, TIME_H, TFT_WHITE);
      
      // フォントの「形」を Font2 (セリフ体) に設定
      M5.Display.setFont(&fonts::Font4);

      // 2. 新しい日付と時刻を描く
      // フォントサイズを 3 に設定 (お好みで調整してください)
      M5.Display.setTextSize(1.75); 
      
      // 1行目: 日付を描画
      M5.Display.drawString(dateStringBuff, TIME_X, TIME_Y);
      
      // 2行目: 時刻を描画 (Y座標をフォントの高さ分(約40px)ずらす)
      M5.Display.drawString(timeStringBuff, TIME_X, TIME_Y + 30);

      // 3. 時計領域「だけ」をEPDに反映
      // これが「部分更新」のコマンドです。画像は更新されません。
      M5.Display.display(TIME_X, TIME_Y, TIME_W, TIME_H);
      
      // シリアルモニタにも時刻を表示
      // Serial.print(dateStringBuff);
      // Serial.print(" ");
      // Serial.println(timeStringBuff);

    } else {
      Serial.println("Failed to get time in loop");
    }
  }
  
  // 他の処理（例: 1分ごとにNTP再同期など）もここに追加できる
  delay(10); // ループがビジーになるのを防ぐ
}
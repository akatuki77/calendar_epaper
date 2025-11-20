// 疎通確認
#include <M5Unified.h>   // M5Stackデバイス(M5Paper S3含む)を統合的に扱うための基本ライブラリ
#include <WiFi.h>        // WiFi接続機能を使うために必要
#include <HTTPClient.h>  // HTTPリクエスト（GETやPOST）を送信するために必要
#include <ArduinoJson.h> // HTTPの応答(JSON形式)を解析するために必要
#include "time.h"       // 時刻取得のために、この行を追加

// --- グローバル変数 (プログラム全体で使う変数) ---

// 接続するWiFiネットワークのSSID（名前）
const char* ssid = "ECCcomp2";
// const char* ssid = "ECCcomp4";

// WiFiのパスワード
const char* password = "4Emah5LdS"; // WiFiパスワード設定

// ステップ1でアクセスする「JSONを返すAPI」のURL
// (Nginx経由でFastAPIの /image エンドポイントを指す)
const char* nginx_url = "http://10.200.2.7:8080/api/image";
// const char* nginx_url = "http://10.200.0.224:8080/api/data";

// return img_byte_arr.getvalue()

// setup()関数は、M5Paperが起動したときに「一度だけ」実行されます
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
  
  // 画面の(10, 10)の位置に文字列を描画（まだEPDには反映されない）
  // M5.Display.drawString("Connecting to WiFi...", 10, 10);
  
  // M5.Display.display() を呼んだ時点で、初めてメモリ上の描画内容がEPDに「反映」される
  // M5.Display.display(); 

  // --- 3. WiFiへの接続 ---
  
  // WiFiに接続を開始
  WiFi.begin(ssid, password);
  
  // シリアルモニタに "Connecting to WiFi" と表示
  // Serial.print("Connecting to WiFi");
  
  // WiFi.status()が WL_CONNECTED (接続完了) になるまで、ループして待つ
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // 0.5秒待つ
    // Serial.print("."); // 待っている間、"." を表示し続ける
  }
  
  // WiFi接続が完了したら、シリアルモニタに "Connected!" と表示
  // Serial.println("\nConnected!");
  
  // 画面の(10, 30)の位置に「接続完了」メッセージを描画（まだ反映されない）
  // M5.Display.drawString("WiFi Connected!", 10, 30);       

  // --- 4. ステップ1: JSON APIへのリクエスト ---
  
  // 画面の(10, 50)の位置に「Nginx接続テスト中」メッセージを描画（まだ反映されない）
  // (コメントはNginxですが、実際はFastAPIへのテストです)
  // M5.Display.drawString("Testing Nginx connection...", 10, 50);
  
  // メモリ上の描画内容（WiFi Connected!など）をEPDに反映
  // M5.Display.display(); 

  // HTTPリクエストを実行するための「HTTPClient」オブジェクト（道具）を準備
  HTTPClient http;
  
  // ステップ2で取得した、描画すべき画像のURLを保存するための変数
  String imageUrlToDraw; 
  String imageUrlToDraw2; 
  
  // シリアルモニタに、これからアクセスするURLを表示
  // Serial.printf("Requesting to: %s\n", nginx_url); 
  
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
    // サーバーからの応答データ（JSON文字列）をすべて取得し、payload変数に保存
    String payload = http.getString();
    
    // 取得したJSON文字列をシリアルモニタに表示
    // Serial.println("Payload: " + payload);

    // ArduinoJsonライブラリを使い、JSON文字列を解析するためのメモリ領域(doc)を準備
    DynamicJsonDocument doc(2048); // 2048バイトのメモリを確保
    
    // payload文字列をJSONとして解析し、結果をdocに入れる
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      // JSONの解析に失敗した場合（例: "Hello Nginx!" などJSONでないものが返ってきた）
      Serial.println("JSON Parse Failed!");
      M5.Display.drawString("JSON Parse Failed!", 10, 50); // 画面にエラー表示
    } else {
      // JSONの解析に成功した場合
      
      // JSON(doc)から "images" というキーの「配列」の「0番目」(1つ目)の要素を、String(文字列)として取り出す
      imageUrlToDraw = doc["images"][0].as<String>(); 
      imageUrlToDraw2 = doc["images"][1].as<String>();
      
      // 取り出した画像URLをシリアルモニタに表示
      // Serial.println("Image URL to draw: " + imageUrlToDraw);
      
      // 画面に「URL取得成功」メッセージを表示
      // M5.Display.drawString("Got Image URL!", 10, 50);
    }
  } else {
    // HTTPリクエストが 200 OK 以外（404 Not Found や -1 Connection Refused など）だった場合
    Serial.printf("JSON API failed, code: %d\n", httpCode);
    M5.Display.drawString("JSON API Failed", 10, 50); // 画面にエラー表示
  }
  
  // 1回目のHTTPリクエスト（JSON取得用）の接続を終了
  http.end();
  
  // EPDに「Got Image URL!」または「JSON Parse Failed!」のメッセージを反映
  // M5.Display.display();

  // --- 6. ステップ3: 画像URLへのリクエスト ---

  // JSON解析が成功して、imageUrlToDraw変数にURLが格納されている場合
  if (imageUrlToDraw.length() > 0) {
    
    // EPDの画面更新は時間がかかるため、少し待機
    delay(500); 
    
    // これから画像を描画するので、画面を一度クリア（白で塗りつぶし）
    M5.Display.fillScreen(TFT_WHITE); 
    
    // 画面に「画像ダウンロード中」メッセージを表示
    // M5.Display.drawString("Downloading image...", 10, 10);
    // M5.Display.display(); // メッセージを反映
    
    // シリアルモニタにもメッセージを表示
    // Serial.println("Requesting Image binary...");
    
    // 2回目のHTTPリクエスト
    // HTTP道具に、今度はJSONから取得した「画像URL (imageUrlToDraw)」をセット
    http.begin(imageUrlToDraw); 
    
    // 再度、HTTP GETリクエストを送信！
    httpCode = http.GET();

    // --- 7. ステップ4: 画像の描画 ---
    
    // 画像のダウンロードが成功 (200 OK) した場合
    if (httpCode == HTTP_CODE_OK) {
      // Serial.println("Image GET success. Drawing...");
      
      // 応答データ（JPEG画像のバイナリデータ）を、すべてpayload変数に読み込む
      String payload = http.getString();

      if (payload.length() == 0) {
        // ダウンロードしたが、中身が空だった場合
        Serial.println("Failed to get payload string.");
        M5.Display.drawString("Image DL Failed (Empty)", 10, 30);
      } else {
        // 画像データの取得に成功した場合
        // Serial.println("Payload received. Drawing image...");
        
        // M5.Display.drawJpg 関数を使い、メモリ(payload)上の画像データを描画
        // bool success = M5.Display.drawJpg( // Jpgの場合
        bool success = M5.Display.drawPng( // Pngの場合
          (const uint8_t*)payload.c_str(), // Stringの中身(const char*)を、バイナリ配列(const uint8_t*)として渡すための「型キャスト」
          payload.length(),               // 画像データの全長（バイト数）
          0,                              // 描画するX座標
          0                               // 描画するY座標
        );

        if(success) {
          // 描画（デコード）に成功した場合
          // Serial.println("Image drawn successfully.");
        } else {
          // 描画（デコード）に失敗した場合（例: データが壊れている、JPG形式ではない）
          Serial.println("Image draw failed (JPG format error?)");
          M5.Display.drawString("Image Draw Failed.", 10, 30);
        }
      }
      
      // メモリ（バッファ）に描画された画像を、EPD（電子ペーパー）に「反映」
      // M5.Display.display(); 

    } else {
      // 画像のダウンロードに失敗した場合 (404 Not Found など)
      Serial.printf("Image download failed, code: %d\n", httpCode);
      M5.Display.drawString("Image DL Failed", 10, 30);
      M5.Display.drawString("Code: " + String(httpCode), 10, 50);
      M5.Display.display();
    }
    
    // 2回目のHTTPリクエスト（画像取得用）の接続を終了
    http.end();
    
  } else {
    // そもそもステップ2のJSON解析に失敗して、画像URLが取得できなかった場合
    Serial.println("Could not get image URL. Halting.");
    M5.Display.drawString("Could not get URL.", 10, 70);
    M5.Display.display();
  }



  if (imageUrlToDraw2.length() > 0) {
    
    // EPDの画面更新は時間がかかるため、少し待機
    delay(500); 
    
    // これから画像を描画するので、画面を一度クリア（白で塗りつぶし）
    // M5.Display.fillScreen(TFT_WHITE); 
    
    // 画面に「画像ダウンロード中」メッセージを表示
    // M5.Display.drawString("Downloading image...", 10, 10);
    // M5.Display.display(); // メッセージを反映
    
    // シリアルモニタにもメッセージを表示
    // Serial.println("Requesting Image binary...");
    
    // 2回目のHTTPリクエスト
    // HTTP道具に、今度はJSONから取得した「画像URL (imageUrlToDraw)」をセット
    http.begin(imageUrlToDraw2); 
    
    // 再度、HTTP GETリクエストを送信！
    httpCode = http.GET();

    // --- 7. ステップ4: 画像の描画 ---
    
    // 画像のダウンロードが成功 (200 OK) した場合
    if (httpCode == HTTP_CODE_OK) {
      // Serial.println("Image GET success. Drawing...");
      
      // 応答データ（JPEG画像のバイナリデータ）を、すべてpayload変数に読み込む
      String payload = http.getString();

      if (payload.length() == 0) {
        // ダウンロードしたが、中身が空だった場合
        Serial.println("Failed to get payload string.");
        M5.Display.drawString("Image DL Failed (Empty)", 10, 30);
      } else {
        // 画像データの取得に成功した場合
        // Serial.println("Payload received. Drawing image...");
        
        // M5.Display.drawJpg 関数を使い、メモリ(payload)上の画像データを描画
        // bool success = M5.Display.drawJpg( // Jpgの場合
        bool success = M5.Display.drawPng( // Pngの場合
          (const uint8_t*)payload.c_str(), // Stringの中身(const char*)を、バイナリ配列(const uint8_t*)として渡すための「型キャスト」
          payload.length(),               // 画像データの全長（バイト数）
          680,                              // 描画するX座標
          0                               // 描画するY座標
        );

        if(success) {
          // 描画（デコード）に成功した場合
          // Serial.println("Image drawn successfully.");
        } else {
          // 描画（デコード）に失敗した場合（例: データが壊れている、JPG形式ではない）
          Serial.println("Image draw failed (JPG format error?)");
          M5.Display.drawString("Image Draw Failed.", 10, 30);
        }
      }
      
      // メモリ（バッファ）に描画された画像を、EPD（電子ペーパー）に「反映」
      // M5.Display.display(); 

    } else {
      // 画像のダウンロードに失敗した場合 (404 Not Found など)
      Serial.printf("Image download failed, code: %d\n", httpCode);
      M5.Display.drawString("Image DL Failed", 10, 30);
      M5.Display.drawString("Code: " + String(httpCode), 10, 50);
      M5.Display.display();
    }
    
    // 2回目のHTTPリクエスト（画像取得用）の接続を終了
    http.end();
    
  } else {
    // そもそもステップ2のJSON解析に失敗して、画像URLが取得できなかった場合
    Serial.println("Could not get image URL. Halting.");
    M5.Display.drawString("Could not get URL.", 10, 70);
    M5.Display.display();
  }



  // --- 8. 最終ステップ: 画面に「一度だけ」反映 ---
  // Serial.println("Both images drawn to buffer. Refreshing screen...");
  
  // 描画した1枚目と2枚目の画像を、ここで「同時に」EPDに反映
  M5.Display.display(); 

  // --- 9. NTP（時計）のセットアップ ---
  Serial.println("Setting up NTP time...");
  
  // 日本のタイムゾーン (JST: 9時間 * 3600秒) を設定
  const long  gmtOffset_sec = 9 * 3600; 
  const int   daylightOffset_sec = 0; // 夏時間はなし
  
  // NTPサーバー（インターネット上の時計サーバー）を設定
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.google.com");

  struct tm timeinfo; // 時刻情報を格納する構造体
  
  // 時刻が取得できるまで待機
  if (!getLocalTime(&timeinfo)) {
    // Serial.println("Failed to obtain time");
    // M5.Display.drawString("NTP Failed", 10, 100); // メモリには描く
  } else {
    // Serial.println("Time obtained.");
    // M5.Display.drawString("NTP OK", 10, 100); // メモリには描く
  }

  // --- 10.最重要 ---
  // これ以降の描画は「部分更新 (epd_fast)」モードで行うよう、モードを切り替える
  // これにより、loop()での display() は「暗転」しなくなります。
  M5.Display.setEpdMode(epd_mode_t::epd_fast); 



  // これで setup() の処理がすべて完了
  // Serial.println("Setup finished.");
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
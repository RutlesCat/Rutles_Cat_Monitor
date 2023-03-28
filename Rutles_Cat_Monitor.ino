//#include "battery.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFiClientSecure.h>

// ##################### Line,Wi-Fi設定(環境設定) #####################
String lineToken          = "**********";   // Lineトークン

const char *ssid        = "**********"; //  Wi-FiのSSID
const char *password    = "**********"; // Wi-Fiのパスワード
// ###################################################################
const char* lineServer = "notify-api.line.me";

// ピン配置
const byte LED_PIN      = 2;  // LED緑

// CAMERA_MODEL_M5_UNIT_CAM
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23

#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    22
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

WiFiClientSecure httpsClient;
bool ledFlag          = true; // LED制御フラグ
camera_fb_t * fb;
  
void setup() {
  Serial.begin(115200);
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable   detector
  //bat_init();
  //bat_hold_output();
  Serial.setDebugOutput(true);
  Serial.println();
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // 画像サイズ設定：QVGA(320x240),CIF(400x296),HVGA(480x320),VGA(640x480),SVGA(800x600),XGA(1024x768)
  config.frame_size = FRAMESIZE_HVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    esp_sleep_enable_timer_wakeup(10 * 1000000);
    esp_deep_sleep_start();
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);

  //*sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 1);       // flip it back
  s->set_brightness(s, 1);  // up the blightness just a bit
  s->set_saturation(s, -2); // lower the saturation*/

  // ####### PIN設定開始 #######
  pinMode ( LED_PIN, OUTPUT );

  // ####### 無線Wi-Fi接続 #######
  WiFi.begin ( ssid, password );
  while ( WiFi.status() != WL_CONNECTED ) { // 接続するまで無限ループ処理
    // 接続中はLEDを１秒毎に点滅する
    ledFlag = !ledFlag;
    digitalWrite(LED_PIN, ledFlag);
    delay ( 1000 );
    Serial.print ( "." );
  }
  // Wi-Fi接続完了（IPアドレス表示）
  Serial.print ( "Wi-Fi Connected! IP address: " );
  Serial.println ( WiFi.localIP() );
  Serial.println ( );
  // Wi-Fi接続時はLED点灯（Wi-Fi接続状態）
  digitalWrite ( LED_PIN, true );

  // ####### 画像取得 #######
  Serial.println("Start get JPG");
  getCameraJPEG();
  // ####### LineへPost #######
  Serial.println("Start Post Line");
  postLine();
  esp_camera_fb_return(fb);

  Serial.println("Line Completed!!!");

  esp_sleep_enable_timer_wakeup(3600000000ULL);
  esp_deep_sleep_start();
  
}

void loop() {
  delay(100);
}

//  画像をPOST処理（送信）
void postLine() {

  // ####### HTTPS証明書設定 #######
  // Serverの証明書チェックをしない（1.0.5以降で必要）【rootCAでルート証明書を設定すれば解決できる】
  httpsClient.setInsecure();//skip verification
  //httpsClient.setCACert(rootCA);// 事前にWebブラウザなどでルート証明書を取得しrootCA設定も可

  Serial.println("Connect to " + String(lineServer));
  if (httpsClient.connect(lineServer, 443)) {
    Serial.println("Connection successful");

    String messageData = "--foo_bar_baz\r\n"
                      "Content-Disposition: form-data; name=\"message\"\r\n\r\n"
                      "ESP32CAM投稿\r\n"; // 表示したいメッセージ
    String startBoundry = "--foo_bar_baz\r\n"
                          "Content-Disposition: form-data; name=\"imageFile\"; filename=\"esp32cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String endBoundry   = "\r\n--foo_bar_baz--";

    unsigned long contentsLength = messageData.length() + startBoundry.length() + fb->len + endBoundry.length();
    String header = "POST /api/notify HTTP/1.0\r\n"
                    "HOST: " + String(lineServer) + "\r\n" +
                    "Connection: close\r\n" +
                    "content-type: multipart/form-data;boundary=foo_bar_baz\r\n" +
                    "content-length: " + String(contentsLength) + "\r\n" +
                    "authorization: Bearer " + lineToken + "\r\n\r\n";

    Serial.println("Send JPEG DATA by API");
    httpsClient.print(header);
    httpsClient.print(messageData);
    httpsClient.print(startBoundry);
    //  JPEGデータは1000bytesに区切ってPOST
    unsigned long dataLength = fb->len;
    uint8_t*      bufAddr    = fb->buf;
    for(unsigned long i = 0; i < dataLength ;i=i+1000) {
      if ( (i + 1000) < dataLength ) {
        httpsClient.write(( bufAddr + i ), 1000);
      } else if (dataLength%1000 != 0) {
        httpsClient.write(( bufAddr + i ), dataLength%1000);
      }
    }
    httpsClient.print(endBoundry);

    Serial.println("Waiting for response.");
    while (httpsClient.connected()) {
      String line = httpsClient.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    while (httpsClient.available()) {
      char c = httpsClient.read();
      Serial.write(c);
    }
  } else {
    Serial.println("Connected to " + String(lineServer) + " failed.");
  }
  httpsClient.stop();
  Serial.println();
  Serial.println("Finish httpsClient");
}

// OV3660でJPEG画像取得
void getCameraJPEG(){
  fb = esp_camera_fb_get();  //JPEG取得
  if (!fb) {
    Serial.printf("Camera capture failed");
  }
  // 撮影終了処理
  Serial.printf("JPG: %uB ", (uint32_t)(fb->len));
  Serial.println();
  
}


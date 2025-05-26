// CameraWebServer.ino

// 必要的 ESP32 系統庫
#include <WiFi.h>
#include <esp_camera.h>

// 鏡頭引腳定義，根據選擇的 CAMERA_MODEL 自動包含
// 這裡我們明確使用 CAMERA_MODEL_AI_THINKER
#define CAMERA_MODEL_AI_THINKER // AI-Thinker ESP32-CAM 模組 (有 PSRAM)
#include "camera_pins.h"

// 自定義模組的頭文件
#include "app_httpd.h"  // 鏡頭 HTTP 伺服器功能
#include "aws_upload.h" // AWS 上傳功能接口

// 新增：NTP 時間同步相關庫
#include <time.h>        // For time functions (time_t, struct tm, time, localtime_r, strftime)
#include "esp_sntp.h"    // For NTP client

// ===========================
// WiFi 連接設定
// ===========================
const char *const WIFI_SSID = "WIFI_SSID";         // 您的 Wi-Fi 名稱
const char *const WIFI_PASSWORD = "WIFI_PASSWORD"; // 您的 Wi-Fi 密碼

// ===========================
// 全局常數和配置
// ===========================
// 鏡頭配置參數的預設值
const int CAMERA_XCLK_FREQ_HZ = 20000000;    // XCLK 時鐘頻率 20 MHz
const framesize_t CAMERA_FRAME_SIZE_DEFAULT = FRAMESIZE_UXGA; // 預設幀大小 UXGA (1600x1200)
const framesize_t CAMERA_FRAME_SIZE_NO_PSRAM = FRAMESIZE_SVGA; // 無 PSRAM 時的幀大小 SVGA (800x600)
const framesize_t CAMERA_FRAME_SIZE_INITIAL_STREAM = FRAMESIZE_QVGA; // 初始串流幀大小 QVGA (320x240)
const pixformat_t CAMERA_PIXEL_FORMAT = PIXFORMAT_JPEG; // 像素格式 JPEG (適合串流)
const int JPEG_QUALITY_DEFAULT = 12;         // 預設 JPEG 壓縮品質 (0-63, 越低品質越好/檔案越大)
const int PSRAM_JPEG_QUALITY = 10;           // 有 PSRAM 時的 JPEG 壓縮品質
const int FB_COUNT_DEFAULT = 1;              // 預設幀緩衝區數量
const int FB_COUNT_PSRAM = 2;                // 有 PSRAM 時的幀緩衝區數量

// ===========================
// NTP 時間同步設定
// ===========================
const char* NTP_SERVER1 = "pool.ntp.org";
const char* NTP_SERVER2 = "time.nist.gov";
const char* TZ_INFO = "CST-8"; // 台灣時區 (Central Standard Time -8, 無日光節約時間調整)

// ===========================
// 函數原型 (為增強可讀性而拆分出的配置函數)
// ===========================
static esp_err_t configureCamera(camera_config_t &config); // 配置鏡頭參數
static void initializeCameraSensor(sensor_t *s);          // 初始化鏡頭傳感器設置
static void connectToWiFi();                              // 連接 WiFi 網絡
static void initTimeSync();                               // 新增：初始化 NTP 時間同步

// ===========================
// setup() 函數：程式初始化
// ===========================
void setup() {
  // 設置串口通訊，用於調試輸出
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n--- ESP32 Camera Web Server 啟動中 ---");

  // 1. 配置鏡頭參數
  camera_config_t cameraConfig;
  if (configureCamera(cameraConfig) != ESP_OK) {
    Serial.println("[Camera] 鏡頭配置失敗！");
    return; // 終止程式執行
  }

  // 2. 初始化鏡頭
  Serial.print("[Camera] 正在初始化鏡頭...");
  esp_err_t err = esp_camera_init(&cameraConfig);
  if (err != ESP_OK) {
    Serial.printf("[Camera] 鏡頭初始化失敗，錯誤碼：0x%x\n", err);
    return; // 終止程式執行
  }
  Serial.println("成功！");

  // 3. 調整鏡頭傳感器設置 (如翻轉、亮度等)
  initializeCameraSensor(esp_camera_sensor_get());

  // 4. 初始化 LED 閃光燈引腳 (如果已定義)
#ifdef LED_GPIO_NUM
  setupLedFlash(LED_GPIO_NUM);
#endif

  // 5. 連接 WiFi
  connectToWiFi();

  // 6. 初始化 NTP 時間同步 (在WiFi連接後執行)
  initTimeSync();

  // 7. 啟動 HTTP 鏡頭伺服器
  startCameraServer();

  // 8. 執行 AWS 上傳服務的初始化 (預留接口)
  aws_upload_setup();

  Serial.printf("[Server] 鏡頭伺服器已就緒！請使用 'http://%s:%d' 連接\n",
                WiFi.localIP().toString().c_str(), HTTP_SERVER_PORT);
}

// ===========================
// loop() 函數：主循環 (閒置)
// ===========================
void loop() {
  // 主循環中不執行任何操作，所有服務都在獨立的 FreeRTOS 任務中運行。
  // 可以考慮在此處添加 WiFi 連線斷開後的重連邏輯，以提高系統穩定性。
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Wi-Fi 連線已斷開，嘗試重新連線...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) { // 等待30秒
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WiFi] 重新連線成功！");
      Serial.printf("[Server] 鏡頭伺服器再次就緒！請使用 'http://%s:%d' 連接\n",
                    WiFi.localIP().toString().c_str(), HTTP_SERVER_PORT);
    } else {
      Serial.println("\n[WiFi] 重新連線失敗，請檢查 Wi-Fi 設置或重啟裝置。");
    }
  }
  delay(5000); // 避免過度檢查，每5秒檢查一次
}

// ===========================
// 輔助函數實現
// ===========================

/**
 * @brief 配置鏡頭初始化參數。
 *
 * @param config camera_config_t 結構體引用，將被填寫。
 * @return esp_err_t 返回 ESP_OK 表示成功，否則為錯誤碼。
 */
static esp_err_t configureCamera(camera_config_t &config) {
  // 清零配置結構體，確保所有成員都有確定的初始值
  memset(&config, 0, sizeof(camera_config_t));

  // LEDC 定時器與通道配置
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  // 數據引腳 (D0-D7)
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  // 時鐘和同步信號引腳
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  // SCCB (I2C) 通訊引腳，用於傳感器配置
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  // 電源和復位引腳
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  // 鏡頭工作頻率
  config.xclk_freq_hz = CAMERA_XCLK_FREQ_HZ;

  // 幀緩衝區設定
  config.pixel_format = CAMERA_PIXEL_FORMAT;
  config.frame_size = CAMERA_FRAME_SIZE_DEFAULT; // 預設使用 UXGA
  config.jpeg_quality = JPEG_QUALITY_DEFAULT;
  config.fb_count = FB_COUNT_DEFAULT;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // 在緩衝區空閒時抓取

  // 根據 PSRAM 存在與否調整配置
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      Serial.println("[Camera] 檢測到 PSRAM，使用更多幀緩衝區和更高品質。");
      config.jpeg_quality = PSRAM_JPEG_QUALITY; // 提高 JPEG 品質
      config.fb_count = FB_COUNT_PSRAM;         // 增加幀緩衝區數量以提高串流流暢度
      config.grab_mode = CAMERA_GRAB_LATEST;    // 抓取最新幀以減少延遲
    } else {
      Serial.println("[Camera] 未檢測到 PSRAM，將幀大小限制為 SVGA。");
      config.frame_size = CAMERA_FRAME_SIZE_NO_PSRAM; // 限制幀大小
      config.fb_location = CAMERA_FB_IN_DRAM;       // 存儲在內部 DRAM
    }
  }
  // 如果是 PIXFORMAT_RGB565 等其他格式，且用於人臉識別等，則設定為 240x240
  else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2; // S3 上人臉識別可以有多個 RGB 緩衝區
#endif
  }
  return ESP_OK;
}

/**
 * @brief 調整鏡頭傳感器的圖像參數。
 *
 * @param s 指向 sensor_t 結構體的指針。
 */
static void initializeCameraSensor(sensor_t *s) {
  // 檢查並調整 OV3660 傳感器的參數
  // AI-Thinker 模組通常使用 OV2640，此段代碼在 OV2640 上不會產生影響
  if (s->id.PID == OV3660_PID) {
    Serial.println("[Camera] OV3660 傳感器檢測到，調整圖像參數。");
    s->set_vflip(s, 1);        // 垂直翻轉圖像
    s->set_brightness(s, 1);   // 稍微增加亮度
    s->set_saturation(s, -2);  // 降低飽和度
  }
  // 對於 AI-Thinker 模組（通常是 OV2640），可能需要手動設置翻轉和鏡像
  // 這裡的設置是基於原始代碼，並適用於多數情況
#if defined(CAMERA_MODEL_AI_THINKER)
  Serial.println("[Camera] AI-Thinker 模組默認設置垂直翻轉和水平鏡像。");
  s->set_vflip(s, 1);    // 垂直翻轉
  s->set_hmirror(s, 1);  // 水平鏡像
#endif

  // 設置初始串流幀大小為 QVGA，以獲得更快的啟動速度
  if (s->set_framesize(s, CAMERA_FRAME_SIZE_INITIAL_STREAM) == ESP_OK) {
    Serial.printf("[Camera] 初始幀大小設置為 %s\n",
                  (CAMERA_FRAME_SIZE_INITIAL_STREAM == FRAMESIZE_QVGA) ? "QVGA" : "未知");
  } else {
    Serial.println("[Camera] 設定初始幀大小失敗。");
  }
}

/**
 * @brief 連接到預設的 WiFi 網絡。
 */
static void connectToWiFi() {
  Serial.print("[WiFi] 正在連接到 Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false); // 禁用 Wi-Fi 睡眠模式以保持連接穩定

  // 等待 WiFi 連接成功
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] 連接成功！");
  Serial.print("[WiFi] IP 地址: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief 初始化 NTP 時間同步。
 */
static void initTimeSync() {
  Serial.println("[NTP] 正在初始化時間同步...");
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
  
  setenv("TZ", TZ_INFO, 1);
  tzset();

  long startWaiting = millis();
  time_t now;
  struct tm timeinfo;
  bool timeSynced = false;

  // 等待時間同步完成，同時檢查 sntp_get_sync_status() 或 getLocalTime()
  while (millis() - startWaiting < 30000) { // 最多等待30秒
    Serial.print(".");
    delay(500);

    // 檢查 sntp_get_sync_status() 是否為 COMPLETED (可能短暫出現)
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      timeSynced = true;
      break; // 立即跳出，表示同步成功
    }

    // 嘗試獲取本地時間，如果成功表示時間已同步
    if (getLocalTime(&timeinfo)) {
      timeSynced = true;
      break; // 立即跳出，表示同步成功
    }
  }

  if (timeSynced) {
    Serial.println("\n[NTP] 時間同步成功！");
    // 再次獲取時間，確保是最新的
    time(&now);
    localtime_r(&now, &timeinfo);
    Serial.printf("[NTP] 當前時間: %s", asctime(&timeinfo));
  } else {
    Serial.println("\n[NTP] 時間同步失敗或超時。時間可能不準確。");
  }
}
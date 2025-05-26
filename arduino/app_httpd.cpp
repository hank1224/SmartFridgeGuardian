// app_httpd.cpp

#include "app_httpd.h"
#include "aws_upload.h" // 包含 AWS 上傳模組，以便在自動拍照時調用上傳接口

#include <Arduino.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <fb_gfx.h> // For potential future overlay/text on image (not used directly in this version)
#include <esp32-hal-ledc.h> // For LEDC control (if needed, not directly used in this version)
#include <sdkconfig.h> // For CONFIG_IDF_TARGET_ESP32S3 etc.

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h" // ESP-IDF 日誌輸出
#endif

// HTTP 伺服器實例句柄
static httpd_handle_t stream_httpd = NULL; // 用於串流伺服器

// ===========================
// HTTP 串流服務相關常量
// ===========================
static const char *const STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char *const STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char *const STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ===========================
// HTTP 請求處理函數
// ===========================

/**
 * @brief 處理實時視頻串流請求 (/stream)。
 *        以 MJPEG 格式向客戶端推送實時圖像。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64]; // 用於構建每個圖像部分的 HTTP 頭

  // 設置 HTTP 響應的 Content-Type 為 MJPEG 串流
  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    Serial.printf("[HTTP] Stream: 設置 Content-Type 失敗 (0x%x)\n", res);
    return res;
  }

  // 無限循環，持續發送圖像幀
  while (true) {
    fb = esp_camera_fb_get(); // 獲取攝像頭幀緩衝區
    if (!fb) {
      Serial.println("[HTTP] Stream: ❌ 無法取得影像幀，串流中斷。");
      res = ESP_FAIL;
      break;
    }

    // 構建每個圖像部分的 HTTP 頭部
    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);

    // 發送 multipart 邊界、圖像頭部和圖像數據
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res != ESP_OK) break;
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res != ESP_OK) break;
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) break;

    esp_camera_fb_return(fb); // 歸還幀緩衝區
  }

  // 確保在循環結束時歸還所有緩衝區
  if (fb) {
    esp_camera_fb_return(fb);
  }
  return res;
}

/**
 * @brief 處理單張圖片捕獲請求 (/capture)。
 *        返回一張 JPEG 格式的圖片。此功能現用於「單次拍照測試」。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t capture_handler(httpd_req_t *req) {
  Serial.println("[HTTP] Capture: 收到單次拍照測試請求。");
  camera_fb_t *fb = esp_camera_fb_get(); // 獲取單張攝像頭幀
  if (!fb) {
    Serial.println("[HTTP] Capture: ❌ 無法取得影像幀，返回 500 錯誤。");
    httpd_resp_send_500(req); // 返回 500 服務器內部錯誤
    return ESP_FAIL;
  }

  // 設置 HTTP 響應頭部
  httpd_resp_set_type(req, "image/jpeg"); // 設置 Content-Type 為 JPEG 圖像
  // 設置 Content-Disposition，讓瀏覽器內聯顯示圖片並建議文件名
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

  // 發送 JPEG 圖像數據
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb); // 歸還幀緩衝區
  return res;
}

/**
 * @brief 處理拍照並上傳請求 (/upload_capture)。
 *        捕獲一張照片並嘗試上傳至 AWS，然後返回操作結果的 HTML 頁面。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t upload_capture_handler(httpd_req_t *req) {
  Serial.println("[HTTP] Upload Capture: 收到拍照並上傳請求。");
  camera_fb_t *fb = esp_camera_fb_get();
  String html_response;

  if (!fb) {
    Serial.println("[HTTP] Upload Capture: ❌ 無法取得影像幀。");
    html_response = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
          <meta charset="UTF-8">
          <title>上傳結果</title>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
              body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
              .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
              h1 { color: #dc3545; } /* Red for error */
              a { display: block; margin-top: 20px; padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; transition: background-color 0.3s ease; }
              a:hover { background-color: #0056b3; }
          </style>
      </head>
      <body>
          <div class="container">
              <h1>錯誤：無法取得影像幀，上傳失敗。</h1>
              <a href='/'>返回主頁</a>
          </div>
      </body>
      </html>
    )rawliteral";
    // 顯式設置 Content-Type 和 charset
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    httpd_resp_sendstr(req, html_response.c_str());
    return ESP_FAIL;
  }

  bool upload_success = uploadPhotoToAWS(fb); // 調用 AWS 上傳接口
  esp_camera_fb_return(fb); // 歸還幀緩衝區

  if (upload_success) {
    Serial.println("[HTTP] Upload Capture: ✅ 照片上傳成功。");
    html_response = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
          <meta charset="UTF-8">
          <title>上傳結果</title>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
              body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
              .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
              h1 { color: #28a745; } /* Green for success */
              a { display: block; margin-top: 20px; padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; transition: background-color 0.3s ease; }
              a:hover { background-color: #0056b3; }
          </style>
      </head>
      <body>
          <div class="container">
              <h1>照片已成功上傳！</h1>
              <a href='/'>返回主頁</a>
          </div>
      </body>
      </html>
    )rawliteral";
  } else {
    Serial.println("[HTTP] Upload Capture: ❌ 照片上傳失敗！請檢查日誌。");
    html_response = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
          <meta charset="UTF-8">
          <title>上傳結果</title>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
              body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
              .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
              h1 { color: #dc3545; } /* Red for error */
              a { display: block; margin-top: 20px; padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; transition: background-color 0.3s ease; }
              a:hover { background-color: #0056b3; }
          </style>
      </head>
      <body>
          <div class="container">
              <h1>照片上傳失敗！請檢查日誌。</h1>
              <a href='/'>返回主頁</a>
          </div>
      </body>
      </html>
    )rawliteral";
  }
  // 顯式設置 Content-Type 和 charset
  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_sendstr(req, html_response.c_str());
  return ESP_OK;
}

/**
 * @brief 處理即時串流子頁面請求 (/stream_page)。
 *        顯示一個包含實時串流圖像的 HTML 頁面。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t stream_page_handler(httpd_req_t *req) {
  const char* html_content = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <title>ESP32-CAM 即時串流</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <style>
            body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; background-color: #f0f0f0; }
            .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
            h1 { color: #333; }
            img { max-width: 100%; height: auto; border: 1px solid #ddd; border-radius: 5px; margin-top: 20px; }
            /* 調整按鈕和連結的樣式 */
            button, a {
              display: block;
              margin: 15px auto; /* 居中 */
              padding: 10px 20px;
              background-color: #007bff;
              color: white;
              text-decoration: none;
              border-radius: 5px;
              transition: background-color 0.3s ease;
              max-width: 250px; /* 限制最大寬度 */
              cursor: pointer; /* 顯示為可點擊 */
            }
            button:hover, a:hover { background-color: #0056b3; }
            #stream-container { margin-top: 20px; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>ESP32-CAM 即時串流</h1>
            <div id="stream-container">
                <img id="stream_img" src="/stream" alt="實時串流">
            </div>
            <button onclick="stopStream()">停止串流</button>
            <a href="/">返回主頁</a>

            <script>
                // JavaScript 函數，用於停止串流
                function stopStream() {
                    var img = document.getElementById('stream_img');
                    img.src = ""; // 清空圖片的 src 屬性，中斷瀏覽器對串流的請求
                    console.log("Stream stopped.");
                }

                // 當頁面加載完成後，確保圖片 src 設置正確 (如果需要重新啟用串流)
                // 這裡的邏輯是為了讓用戶每次進入頁面時都能看到串流，除非他們點擊停止
                window.onload = function() {
                    var img = document.getElementById('stream_img');
                    // 如果圖片 src 為空（可能是因為之前停止了串流），則重新載入
                    if (!img.src || img.src === window.location.origin + "/") { // 檢查是否為空或根路徑
                        img.src = "/stream";
                    }
                };
            </script>
        </div>
    </body>
    </html>
  )rawliteral";

  // 顯式設置 Content-Type 和 charset
  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_send(req, html_content, strlen(html_content));
  return ESP_OK;
}

/**
 * @brief 處理根路徑請求 (/)。
 *        返回一個簡潔的 HTML 頁面，提供三個子功能連結。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t root_handler(httpd_req_t *req) {
  const char* html_content = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <title>ESP32-CAM 主頁</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <style>
            body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
            .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
            h1 { color: #333; }
            a { display: block; margin: 15px auto; padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; transition: background-color 0.3s ease; max-width: 250px; }
            a:hover { background-color: #0056b3; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>ESP32-CAM 網頁伺服器</h1>
            <p>請選擇功能：</p>
            <a href="/stream_page">1. 即時串流</a>
            <a href="/capture">2. 單次拍照測試</a>
            <a href="/upload_capture">3. 拍照並上傳</a>
        </div>
    </body>
    </html>
  )rawliteral";

  // 顯式設置 Content-Type 和 charset
  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_send(req, html_content, strlen(html_content));
  return ESP_OK;
}


// ===========================
// HTTP 伺服器初始化
// ===========================

/**
 * @brief 啟動攝像頭的 HTTP 伺服器。
 *        包含實時串流 (/stream), 獨立串流頁面 (/stream_page),
 *        單張圖片捕獲 (/capture) 和拍照並上傳 (/upload_capture) 服務。
 */
void startCameraServer() {

  // HTTP 伺服器配置
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_SERVER_PORT; // 設置伺服器端口為 81

  // 定義 URI 處理器
  httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL
  };

  httpd_uri_t stream_page_uri = { // 新增：用於顯示即時串流的獨立 HTML 頁面
    .uri = "/stream_page",
    .method = HTTP_GET,
    .handler = stream_page_handler,
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = { // 原有的 MJPEG 串流數據源
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  httpd_uri_t capture_uri = { // 現用於「單次拍照測試」
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = capture_handler,
    .user_ctx = NULL
  };

  httpd_uri_t upload_capture_uri = { // 新增：用於「拍照並上傳」
    .uri = "/upload_capture",
    .method = HTTP_GET,
    .handler = upload_capture_handler,
    .user_ctx = NULL
  };

  Serial.printf("[Server] 正在啟動 HTTP 伺服器，端口：%d\n", HTTP_SERVER_PORT);
  // 啟動 HTTP 伺服器並註冊 URI 處理器
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &root_uri);
    httpd_register_uri_handler(stream_httpd, &stream_page_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &capture_uri);
    httpd_register_uri_handler(stream_httpd, &upload_capture_uri);
    Serial.println("[Server] ✅ / (根目錄), /stream_page, /stream, /capture, /upload_capture HTTP 服務啟動成功。");
  } else {
    Serial.println("[Server] ❌ 無法啟動 HTTP 伺服器！");
  }
}

/**
 * @brief 初始化 LED 閃光燈引腳。
 *        將引腳設置為輸出模式並確保其初始為關閉狀態。
 * @param pin LED 連接的 GPIO 引腳號。
 */
void setupLedFlash(int pin) {
  Serial.printf("[LED] 設置 LED 閃光燈引腳 %d\n", pin);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW); // 確保 LED 初始為關閉狀態
  // 如果需要更精細的控制，可以考慮在這裡初始化 LEDC
  // ledcSetup(LEDC_CHANNEL_1, 5000, 8); // 設置 PWM 通道
  // ledcAttachPin(pin, LEDC_CHANNEL_1);
}

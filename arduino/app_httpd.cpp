// app_httpd.cpp

#include "app_httpd.h"
#include "aws_upload.h" // 包含 AWS 上傳模組，以便在自動拍照時調用上傳接口

#include <Arduino.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <fb_gfx.h>
#include <esp32-hal-ledc.h> // For LEDC control (if needed, not directly used in this version)
#include <sdkconfig.h>

#include <time.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif

// HTTP 伺服器實例句柄
static httpd_handle_t stream_httpd = NULL;

// 設備 ID (可根據實際設備進行調整)
const char* DEVICE_ID = "fridge_1";

// 前向聲明：Base64 編碼函數
String base64Encode(const unsigned char *data, size_t len);

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
 * @brief 處理 API 請求：實時視頻串流數據 (/api/stream)。
 *        以 MJPEG 格式向客戶端推送實時圖像。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t handle_api_stream_data(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    Serial.printf("[API] Stream: 設置 Content-Type 失敗 (0x%x)\n", res);
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[API] Stream: ❌ 無法取得影像幀，串流中斷。");
      res = ESP_FAIL;
      break;
    }

    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res != ESP_OK) break;
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res != ESP_OK) break;
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) break;

    esp_camera_fb_return(fb);
  }

  if (fb) {
    esp_camera_fb_return(fb);
  }
  return res;
}

/**
 * @brief 處理 API 請求：單次拍照並返回原始 JPEG 圖片數據 (/api/capture_single)。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t handle_api_capture_single_image(httpd_req_t *req) {
  Serial.println("[API] /api/capture_single: 收到單次拍照請求。");
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_FAIL; // 預設失敗

#ifdef LED_GPIO_NUM
  Serial.println("[API] 📸 開啟閃光燈...");
  digitalWrite(LED_GPIO_NUM, HIGH); // 開啟閃光燈
  delay(100); // 延遲，讓閃光燈達到亮度峰值
#endif

  fb = esp_camera_fb_get(); // 獲取單張鏡頭幀

#ifdef LED_GPIO_NUM
  Serial.println("[API] 關閉閃光燈...");
  digitalWrite(LED_GPIO_NUM, LOW); // 關閉閃光燈
#endif

  if (!fb) {
    Serial.println("[API] /api/capture_single: ❌ 無法取得影像幀，返回 500 錯誤。");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
  return res;
}

/**
 * @brief 處理 API 請求：拍照並上傳到 AWS，返回 JSON 狀態 (/api/upload_photo)。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t handle_api_upload_photo(httpd_req_t *req) {
  Serial.println("[API] /api/upload_photo: 收到拍照並上傳請求。");
  camera_fb_t *fb = NULL;
  String json_response_content;

#ifdef LED_GPIO_NUM
  Serial.println("[API] 📸 開啟閃光燈...");
  digitalWrite(LED_GPIO_NUM, HIGH); // 開啟閃光燈
  delay(100); // 延遲，讓閃光燈達到亮度峰值
#endif

  fb = esp_camera_fb_get(); // 獲取單張鏡頭幀

#ifdef LED_GPIO_NUM
  Serial.println("[API] 關閉閃光燈...");
  digitalWrite(LED_GPIO_NUM, LOW); // 關閉閃光燈
#endif

  if (!fb) {
    Serial.println("[API] /api/upload_photo: ❌ 無法取得影像幀。");
    httpd_resp_set_type(req, "application/json; charset=UTF-8");
    httpd_resp_set_status(req, "503 Service Unavailable");
    json_response_content = "{\"status\":\"error\", \"message\":\"Failed to get camera frame\"}";
    httpd_resp_sendstr(req, json_response_content.c_str());
    return ESP_FAIL;
  }

  bool upload_success = uploadPhotoToAWS(fb); // 調用 AWS 上傳接口
  esp_camera_fb_return(fb); // 歸還幀緩衝區

  httpd_resp_set_type(req, "application/json; charset=UTF-8");
  if (upload_success) {
    Serial.println("[API] /api/upload_photo: ✅ 照片上傳成功。");
    json_response_content = "{\"status\":\"success\", \"message\":\"Photo uploaded successfully\"}";
  } else {
    Serial.println("[API] /api/upload_photo: ❌ 照片上傳失敗！請檢查日誌。");
    json_response_content = "{\"status\":\"error\", \"message\":\"Failed to upload photo to AWS\"}";
    httpd_resp_set_status(req, "500 Internal Server Error");
  }
  httpd_resp_sendstr(req, json_response_content.c_str());
  return ESP_OK;
}

/**
 * @brief 處理 API 請求：拍照並返回 Base64 編碼的圖片和元數據 (/api/photos)。
 *        遵循 RESTful 規範，返回 JSON 格式。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t handle_api_get_photo_with_meta(httpd_req_t *req) {
  Serial.println("[API] GET /api/photos: 收到拍照並返回元數據請求。");
  camera_fb_t *fb = NULL;
  String json_response;

#ifdef LED_GPIO_NUM
  Serial.println("[API] 📸 開啟閃光燈...");
  digitalWrite(LED_GPIO_NUM, HIGH); // 開啟閃光燈
  delay(100); // 延遲，讓閃光燈達到亮度峰值
#endif

  fb = esp_camera_fb_get(); // 獲取單張鏡頭幀

#ifdef LED_GPIO_NUM
  Serial.println("[API] 關閉閃光燈...");
  digitalWrite(LED_GPIO_NUM, LOW); // 關閉閃光燈
#endif

  if (!fb) {
    Serial.println("[API] GET /api/photos: ❌ 無法取得影像幀。");
    httpd_resp_set_type(req, "application/json; charset=UTF-8");
    httpd_resp_set_status(req, "503 Service Unavailable");
    json_response = "{\"status\":\"error\", \"message\":\"Failed to get camera frame\"}";
    httpd_resp_sendstr(req, json_response.c_str());
    return ESP_FAIL;
  }

  // 獲取時間戳
  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // 格式化時間為 ISO 8601 格式：YYYY-MM-DDTHH:MM:SSZ
  strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  // Base64 編碼圖片數據
  String base64_image = base64Encode(fb->buf, fb->len);

  // 構建 JSON 響應
  json_response = "{";
  json_response += "\"id\": \"" + String(DEVICE_ID) + "\",";
  json_response += "\"timestamp\": \"" + String(strftime_buf) + "\",";
  json_response += "\"image_base64\": \"" + base64_image + "\",";
  json_response += "\"content_type\": \"image/jpeg\"";
  json_response += "}";

  esp_camera_fb_return(fb);

  // 設置 HTTP 響應頭部為 JSON
  httpd_resp_set_type(req, "application/json; charset=UTF-8");
  httpd_resp_sendstr(req, json_response.c_str());

  Serial.println("[API] GET /api/photos: ✅ 圖片和元數據已返回。");
  return ESP_OK;
}


/**
 * @brief 處理 UI 請求：即時串流顯示頁面 (/ui/stream)。
 *        顯示一個包含實時串流圖像和控制按鈕的 HTML 頁面。
 * @param req HTTP 請求對象。
 * @return esp_err_t 請求處理結果。
 */
static esp_err_t serve_ui_stream_page(httpd_req_t *req) {
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
            button, a {
              display: block;
              margin: 15px auto;
              padding: 10px 20px;
              background-color: #007bff;
              color: white;
              text-decoration: none;
              border-radius: 5px;
              transition: background-color 0.3s ease;
              max-width: 250px;
              cursor: pointer;
              border: none;
            }
            button:hover, a:hover { background-color: #0056b3; }
            #stream-container { margin-top: 20px; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>ESP32-CAM 即時串流</h1>
            <div id="stream-container">
                <img id="stream_img" src="/api/stream" alt="實時串流"> <!-- 指向 API 端點 -->
            </div>
            <button onclick="stopStream()">停止串流</button>
            <a href="/">返回主頁</a>

            <script>
                function stopStream() {
                    var img = document.getElementById('stream_img');
                    img.src = ""; // 清空圖片的 src 屬性，中斷瀏覽器對串流的請求
                    console.log("Stream stopped.");
                }
                window.onload = function() {
                    var img = document.getElementById('stream_img');
                    if (!img.src || img.src === window.location.origin + "/") {
                        img.src = "/api/stream"; // 確保重新載入時仍指向 API 端點
                    }
                };
            </script>
        </div>
    </body>
    </html>
  )rawliteral";

  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_send(req, html_content, strlen(html_content));
  return ESP_OK;
}

/**
 * @brief 處理 UI 請求：單次拍照測試頁面 (/ui/test_capture)。
 *        提供一個按鈕觸發拍照並顯示結果。
 * @param req HTTP 請求對象。
 * @return esp_err_t 這個函數返回 ESP_OK 或 ESP_FAIL。
 */
static esp_err_t serve_ui_test_capture_page(httpd_req_t *req) {
  const char* html_content = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <title>單次拍照測試</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <style>
            body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
            .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
            h1 { color: #333; }
            button, a {
              display: block;
              margin: 15px auto;
              padding: 10px 20px;
              background-color: #007bff;
              color: white;
              text-decoration: none;
              border-radius: 5px;
              transition: background-color 0.3s ease;
              max-width: 250px;
              cursor: pointer;
              border: none;
            }
            button:hover, a:hover { background-color: #0056b3; }
            img { max-width: 100%; height: auto; border: 1px solid #ddd; border-radius: 5px; margin-top: 20px; }
            #loading { color: #666; margin-top: 10px; display: none; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>單次拍照測試</h1>
            <button onclick="takeSnapshot()">拍攝照片</button>
            <p id="loading">載入中，請稍候...</p>
            <img id="snapshot_img" src="" alt="拍攝結果">
            <a href="/">返回主頁</a>

            <script>
                async function takeSnapshot() {
                    var img = document.getElementById('snapshot_img');
                    var loading = document.getElementById('loading');
                    img.src = ""; // 清空上次的圖片
                    loading.style.display = 'block'; // 顯示載入提示

                    try {
                        // 向 API 端點請求單張圖片
                        const response = await fetch('/api/capture_single');
                        if (response.ok) {
                            const blob = await response.blob();
                            img.src = URL.createObjectURL(blob); // 將圖片 Blob 顯示為 URL
                            img.onload = () => URL.revokeObjectURL(img.src); // 圖片載入後釋放 Blob URL
                        } else {
                            console.error('Failed to capture image:', response.status, response.statusText);
                            alert('拍攝失敗！請檢查設備或重新整理。');
                        }
                    } catch (error) {
                        console.error('Error fetching image:', error);
                        alert('網路或設備錯誤，拍攝失敗！');
                    } finally {
                        loading.style.display = 'none'; // 隱藏載入提示
                    }
                }
            </script>
        </div>
    </body>
    </html>
  )rawliteral";

  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_send(req, html_content, strlen(html_content));
  return ESP_OK;
}

/**
 * @brief 處理 UI 請求：拍照並上傳頁面 (/ui/upload)。
 *        提供一個按鈕觸發拍照並上傳，並顯示結果。
 * @param req HTTP 請求對象。
 * @return esp_err_t 這個函數返回 ESP_OK 或 ESP_FAIL。
 */
static esp_err_t serve_ui_upload_page(httpd_req_t *req) {
  const char* html_content = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <title>拍照並上傳</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <style>
            body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
            .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
            h1 { color: #333; }
            button, a {
              display: block;
              margin: 15px auto;
              padding: 10px 20px;
              background-color: #007bff;
              color: white;
              text-decoration: none;
              border-radius: 5px;
              transition: background-color 0.3s ease;
              max-width: 250px;
              cursor: pointer;
              border: none;
            }
            button:hover, a:hover { background-color: #0056b3; }
            #status-message { margin-top: 20px; font-weight: bold; }
            .success { color: #28a745; }
            .error { color: #dc3545; }
            #loading { color: #666; margin-top: 10px; display: none; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>拍照並上傳至 AWS</h1>
            <button onclick="uploadPhoto()">拍攝並上傳</button>
            <p id="loading">上傳中，請稍候...</p>
            <div id="status-message"></div>
            <a href="/">返回主頁</a>

            <script>
                async function uploadPhoto() {
                    var statusDiv = document.getElementById('status-message');
                    var loading = document.getElementById('loading');
                    statusDiv.innerHTML = ""; // 清空上次的狀態
                    loading.style.display = 'block'; // 顯示載入提示

                    try {
                        // 向 API 端點請求拍照並上傳
                        const response = await fetch('/api/upload_photo');
                        const data = await response.json(); // 解析 JSON 響應

                        if (response.ok && data.status === 'success') {
                            statusDiv.className = 'success';
                            statusDiv.innerHTML = '✅ ' + data.message;
                        } else {
                            statusDiv.className = 'error';
                            statusDiv.innerHTML = '❌ ' + (data.message || '未知錯誤，請檢查串口日誌。');
                        }
                    } catch (error) {
                        statusDiv.className = 'error';
                        statusDiv.innerHTML = '❌ 網路錯誤，無法連接設備或解析響應。';
                        console.error('Error uploading photo:', error);
                    } finally {
                        loading.style.display = 'none'; // 隱藏載入提示
                    }
                }
            </script>
        </div>
    </body>
    </html>
  )rawliteral";

  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_send(req, html_content, strlen(html_content));
  return ESP_OK;
}

/**
 * @brief 處理根路徑請求 (/)。
 *        返回一個簡潔的 HTML 頁面，提供所有主要功能的連結。
 * @param req HTTP 請求對象。
 * @return esp_err_t 這個函數返回 ESP_OK。
 */
static esp_err_t serve_ui_root_page(httpd_req_t *req) {
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
            <a href="/ui/stream">1. 即時串流</a>
            <a href="/ui/test_capture">2. 單次拍照測試</a>
            <a href="/ui/upload">3. 拍照並上傳</a>
            <p>API 端點 (供程式呼叫):</p>
            <pre><a href="/api/photos">`/api/photos`</a></pre>
        </div>
    </body>
    </html>
  )rawliteral";

  httpd_resp_set_type(req, "text/html; charset=UTF-8");
  httpd_resp_send(req, html_content, strlen(html_content));
  return ESP_OK;
}


// ===========================
// HTTP 伺服器初始化
// ===========================

/**
 * @brief 啟動鏡頭的 HTTP 伺服器。
 *        註冊所有 UI 頁面和 API 端點的 URI 處理器。
 */
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_SERVER_PORT;

  // --- UI 頁面 URI 處理器 ---
  httpd_uri_t root_ui_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = serve_ui_root_page,
    .user_ctx = NULL
  };

  httpd_uri_t stream_ui_uri = {
    .uri = "/ui/stream",
    .method = HTTP_GET,
    .handler = serve_ui_stream_page,
    .user_ctx = NULL
  };

  httpd_uri_t test_capture_ui_uri = {
    .uri = "/ui/test_capture",
    .method = HTTP_GET,
    .handler = serve_ui_test_capture_page,
    .user_ctx = NULL
  };

  httpd_uri_t upload_ui_uri = {
    .uri = "/ui/upload",
    .method = HTTP_GET,
    .handler = serve_ui_upload_page,
    .user_ctx = NULL
  };

  // --- API 端點 URI 處理器 ---
  httpd_uri_t api_stream_data_uri = {
    .uri = "/api/stream",
    .method = HTTP_GET,
    .handler = handle_api_stream_data,
    .user_ctx = NULL
  };

  httpd_uri_t api_capture_single_image_uri = {
    .uri = "/api/capture_single",
    .method = HTTP_GET,
    .handler = handle_api_capture_single_image,
    .user_ctx = NULL
  };

  httpd_uri_t api_upload_photo_uri = {
    .uri = "/api/upload_photo",
    .method = HTTP_GET,
    .handler = handle_api_upload_photo,
    .user_ctx = NULL
  };

  httpd_uri_t api_get_photo_with_meta_uri = {
    .uri = "/api/photos",
    .method = HTTP_GET,
    .handler = handle_api_get_photo_with_meta,
    .user_ctx = NULL
  };


  Serial.printf("[Server] 正在啟動 HTTP 伺服器，端口：%d\n", HTTP_SERVER_PORT);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    // 註冊 UI 頁面處理器
    httpd_register_uri_handler(stream_httpd, &root_ui_uri);
    httpd_register_uri_handler(stream_httpd, &stream_ui_uri);
    httpd_register_uri_handler(stream_httpd, &test_capture_ui_uri);
    httpd_register_uri_handler(stream_httpd, &upload_ui_uri);

    // 註冊 API 端點處理器
    httpd_register_uri_handler(stream_httpd, &api_stream_data_uri);
    httpd_register_uri_handler(stream_httpd, &api_capture_single_image_uri);
    httpd_register_uri_handler(stream_httpd, &api_upload_photo_uri);
    httpd_register_uri_handler(stream_httpd, &api_get_photo_with_meta_uri);


    Serial.println("[Server] ✅ 所有 HTTP 服務已啟動。");
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

// ===========================
// Base64 編碼實現 (簡化版)
// ===========================
static const char* base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64Encode(const unsigned char *data, size_t len) {
    String encoded_string = "";
    int i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i < 4) ; i++)
                encoded_string += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            encoded_string += base64_chars[char_array_4[j]];

        while((i++ < 3))
            encoded_string += '=';
    }

    return encoded_string;
}
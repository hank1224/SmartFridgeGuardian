// aws_upload.cpp

#include "aws_upload.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <esp_log.h>

// ===========================
// 日誌標籤定義
// ===========================
static const char *TAG_AWS = "AWS_Upload";

// --- AWS S3 上傳 API Gateway 端點 URL ---
const char* AWS_UPLOAD_ENDPOINT = "https://xxxxxxxxxx.execute-api.your-region.amazonaws.com/prod/upload";

// --- AWS API 金鑰 ---
const char* AWS_API_KEY = "YOUR_AWS_GATEWAY_API_KEY";

/**
 * @brief 執行 AWS 相關的初始化設定。
 *        例如：AWS SDK 客戶端初始化、證書加載等。
 */
void aws_upload_setup() {
  ESP_LOGI(TAG_AWS, "AWS 上傳服務初始化接口被調用。");
}

/**
 * @brief 將攝像頭拍攝到的照片幀緩衝區上傳至 AWS S3 (通過 API Gateway)。
 *        此函數使用 HTTP POST 請求將 JPEG 數據發送到 API Gateway 端點。
 * @param fb 指向 camera_fb_t 結構體的指針，包含圖像數據。
 * @return bool 返回 true 表示上傳操作成功，false 表示失敗。
 */
bool uploadPhotoToAWS(camera_fb_t *fb) {
  if (!fb) {
    ESP_LOGE(TAG_AWS, "錯誤：接收到空幀緩衝區，無法上傳。");
    return false;
  }

  if (strcmp(AWS_UPLOAD_ENDPOINT, "https://xxxxxxxxxx.execute-api.your-region.amazonaws.com/prod/upload") == 0 || String(AWS_UPLOAD_ENDPOINT).isEmpty()) {
    ESP_LOGE(TAG_AWS, "錯誤：AWS API Gateway 上傳 URL 未設定。請替換為實際 URL。");
    return false;
  }
  if (strcmp(AWS_API_KEY, "YOUR_AWS_GATEWAY_API_KEY") == 0 || String(AWS_API_KEY).isEmpty()) {
    ESP_LOGE(TAG_AWS, "錯誤：AWS API 金鑰未設定。請替換為實際金鑰。");
    return false;
  }


  ESP_LOGI(TAG_AWS, "正在嘗試上傳 %u bytes 的照片到 AWS API Gateway...", fb->len);
  ESP_LOGI(TAG_AWS, "目標 URL: %s", AWS_UPLOAD_ENDPOINT);

  HTTPClient http;
  http.begin(AWS_UPLOAD_ENDPOINT);

  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("x-api-key", AWS_API_KEY);

  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    ESP_LOGI(TAG_AWS, "API Gateway 響應碼：%d", httpResponseCode);
    if (httpResponseCode == HTTP_CODE_OK) {
      ESP_LOGI(TAG_AWS, "照片成功發送到 API Gateway！");
      String response = http.getString();
      ESP_LOGI(TAG_AWS, "響應內容: %s", response.c_str());
      http.end();
      return true;
    } else {
      String response = http.getString();
      ESP_LOGE(TAG_AWS, "上傳失敗，API Gateway 錯誤碼：%d，訊息：%s", httpResponseCode, response.c_str());
      http.end();
      return false;
    }
  } else {
    ESP_LOGE(TAG_AWS, "HTTP 請求失敗，錯誤：%s", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
}
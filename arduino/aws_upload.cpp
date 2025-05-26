// aws_upload.cpp

#include "aws_upload.h"
#include <Arduino.h> // 為了 Serial.println
#include <HTTPClient.h> // 引入 HTTPClient 庫用於發送 HTTP 請求

// --- AWS S3 上傳 API Gateway 端點 URL ---
// 請將此處替換為您從 AWS API Gateway 取得的「叫用 URL」
const char* AWS_UPLOAD_ENDPOINT = "https://xxxxxxxxxx.execute-api.your-region.amazonaws.com/prod/upload";

// --- AWS API 金鑰 ---
// 務必確保這是 API Gateway 生成的金鑰。
const char* AWS_API_KEY = "esp32-cam-api-key";

// --- AWS SDK 初始化佔位符 ---
void aws_upload_setup() {
  Serial.println("[AWS] AWS 上傳服務初始化接口被調用。");
  // 對於透過 API Gateway 上傳，通常不需要特殊的 AWS SDK 初始化在 ESP32 端。
  Serial.println("[AWS] AWS 初始化（佔位符）完成。");
}

/**
 * @brief 將攝像頭拍攝到的照片幀緩衝區上傳至 AWS S3 (通過 API Gateway)。
 *        此函數使用 HTTP POST 請求將 JPEG 數據發送到 API Gateway 端點，並包含 API 金鑰。
 * @param fb 指向 camera_fb_t 結構體的指針，包含圖像數據。
 * @return bool 返回 true 表示上傳操作成功，false 表示失敗。
 */
bool uploadPhotoToAWS(camera_fb_t *fb) {
  if (!fb) {
    Serial.println("[AWS] 錯誤：接收到空幀緩衝區，無法上傳。");
    return false;
  }

  // 檢查是否已配置上傳端點和 API 金鑰
  if (strcmp(AWS_UPLOAD_ENDPOINT, "https://xxxxxxxxxx.execute-api.your-region.amazonaws.com/prod/upload") == 0 || String(AWS_UPLOAD_ENDPOINT).isEmpty()) {
    Serial.println("[AWS] 錯誤：AWS API Gateway 上傳 URL 未設定。請替換為實際 URL。");
    esp_camera_fb_return(fb); // 確保歸還緩衝區
    return false;
  }
  if (strcmp(AWS_API_KEY, "esp32-cam-api-key") == 0 || String(AWS_API_KEY).isEmpty()) {
    Serial.println("[AWS] 錯誤：AWS API 金鑰未設定。請替換為實際金鑰。");
    esp_camera_fb_return(fb); // 確保歸還緩衝區
    return false;
  }


  Serial.printf("[AWS] 正在嘗試上傳 %u bytes 的照片到 AWS API Gateway...\n", fb->len);
  Serial.printf("[AWS] 目標 URL: %s\n", AWS_UPLOAD_ENDPOINT);

  HTTPClient http;
  http.begin(AWS_UPLOAD_ENDPOINT); // 使用 API Gateway 端點初始化 HTTP 客戶端

  // 設置 Content-Type 為 image/jpeg
  http.addHeader("Content-Type", "image/jpeg");

  // **新增：添加 x-api-key 請求頭以進行驗證**
  http.addHeader("x-api-key", AWS_API_KEY);


  // 發送 HTTP POST 請求並上傳圖片數據
  int httpResponseCode = http.POST(fb->buf, fb->len);

  // 檢查 HTTP 響應碼
  if (httpResponseCode > 0) {
    Serial.printf("[AWS] API Gateway 響應碼：%d\n", httpResponseCode);
    if (httpResponseCode == HTTP_CODE_OK) { // HTTP_CODE_OK == 200
      Serial.println("[AWS] ✅ 照片成功發送到 API Gateway！");
      // 您可以選擇獲取 API Gateway / Lambda 返回的響應內容
      String response = http.getString();
      Serial.printf("[AWS] 響應內容: %s\n", response.c_str()); // 打印 Lambda 的響應
      http.end(); // 關閉連接
      return true;
    } else {
      String response = http.getString();
      Serial.printf("[AWS] ❌ 上傳失敗，API Gateway 錯誤碼：%d，訊息：%s\n", httpResponseCode, response.c_str());
      http.end(); // 關閉連接
      return false;
    }
  } else {
    Serial.printf("[AWS] ❌ HTTP 請求失敗，錯誤：%s\n", http.errorToString(httpResponseCode).c_str());
    http.end(); // 關閉連接
    return false;
  }
}
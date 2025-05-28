# ESP32-CAM 規格書

### 1. 簡介

本專案實現在 ESP32-CAM 微控制器模組上運行一個輕量級的網路應用。該應用核心功能為根據後端指令或內部觸發機制進行拍照，並將圖像數據（通常為 JPEG 格式）連同相關元數據傳送至雲端後端系統進行處理。系統設計上支援兩種主要的圖像傳送模式，以滿足不同部署場景的需求。

### 2. 硬體需求

*   ESP32-CAM 開發板 (例如：AI-Thinker ESP32-CAM 或其他支援的型號)
*   OV2640 或 OV3660 攝像頭模組 (通常隨開發板附帶)
*   Micro USB 線 (用於供電和程式燒錄)
*   5V 供電電源
*   (選配) LED 閃光燈 (如果開發板上定義了 `LED_GPIO_NUM`)
*   (選配) 門感應器或其他觸發裝置 (用於未來擴展或模式一的非後端觸發)

### 3. 軟體與環境需求

*   Arduino IDE 或其他相容的 ESP32 開發環境 (例如：VS Code + PlatformIO)
*   安裝必要的 ESP32 板支援套件
*   安裝必要的程式庫 (`esp_camera`, `WiFi`, `esp_http_server`, `HTTPClient`, `esp_log`, `time.h`, `esp_sntp.h`, Base64 編碼庫)
*   有效的 Wi-Fi 網路連線，且需要具備與後端系統或雲端服務（AWS API Gateway）通訊的能力。
*   (若需 AWS 上傳功能) 已配置的 AWS API Gateway 端點 URL 和 API Key

### 4. 功能規格

#### 4.1. 核心功能

*   **攝像頭初始化與配置:** 支援根據 PSRAM 是否存在自動調整幀緩衝區數量、JPEG 品質和抓取模式，並可設定初始串流幀大小。支援不同 ESP32-CAM 型號的引腳定義。
*   **Wi-Fi 連接:** 連接到指定的 SSID 和密碼的 Wi-Fi 網路，並包含基礎的斷線重連機制。
*   **NTP 時間同步:** 透過 NTP 服務器同步設備時間，確保圖片元數據中的時間戳準確性 (UTC 時間)。
*   **HTTP 伺服器:** 運行一個基於 `esp_http_server` 的 Web 伺服器，用於提供狀態、測試頁面，以及 **接收後端的主動拉取請求或觸發信號 (取決於配置模式)**。
*   **HTTP 客戶端:** 使用 `HTTPClient` 庫來 **主動向雲端 API Gateway 發送圖像數據 (模式一)** 或向後端特定端點發送狀態/通知 (未來可能)。
*   **圖像捕獲與編碼:** 捕獲單張 JPEG 圖像，並能將其數據進行 Base64 編碼。
*   **LED 閃光燈控制:** (如果硬體支援並已配置引腳) 在拍照時暫時開啟 LED 閃光燈。
*   **設備 ID 管理:** 內建一個唯一的設備 ID (`DEVICE_ID`)，用於標識設備並在傳送數據時包含。

#### 4.2. Web 介面 (UI)

提供簡潔的 HTML 頁面供使用者透過瀏覽器訪問和操作，這些介面主要用於開發測試和手動操作，**非系統正常運作時後端主要互動方式**：

*   `/` (根目錄): 主頁，提供指向其他功能頁面的連結和設備狀態概覽 (IP 地址, SSID)。
*   `/ui/stream`: 顯示即時 MJPEG 視訊串流的頁面，包含停止串流的按鈕。
*   `/ui/test_capture`: 單次拍照測試頁面，點擊按鈕觸發拍照並在頁面上顯示拍攝到的圖片。
*   `/ui/test_upload`: 拍照並上傳測試頁面，點擊按鈕觸發拍照並根據配置模式執行上傳流程（可能是模式一的 AWS 上傳或模式二的模擬後端拉取響應），顯示結果狀態。

#### 4.3. API 接口 (與後端互動的關鍵)

根據系統配置的圖像採集模式，ESP32-CAM 需要實現以下 API 接口：

*   **4.3.1. ESP32 作為 HTTP 伺服器 (用於模式二：後端主動拉取 & 可能用於模式一的觸發)**
    *   **端點：** `GET http://[ESP32_CAM_IP]:[HTTP_SERVER_PORT]/api/photos`
        *   **功能：** 接收來自 Django 後端系統的 HTTP GET 請求。接收到請求後，ESP32-CAM 應執行拍照流程 (開閃光燈 -> 拍照 -> 關閃光燈)，將捕獲的 JPEG 圖像進行 Base64 編碼，並連同設備 ID (`DEVICE_ID`)、當前時間戳 (UTC 時間，從 NTP 同步獲取) 和內容類型 (`image/jpeg`)，組合成 JSON 格式的響應返回給後端。
        *   **請求：** HTTP GET 請求，可能不包含參數。未來可以擴展接收參數 (如期望解析度、品質等)。
        *   **響應 (成功 `200 OK`)：** JSON 格式數據。
            ```json
            {
              "id": "...", // 設備的唯一 ID (DEVICE_ID)
              "timestamp": "YYYY-MM-DDTHH:MM:SSZ", // UTC 時間戳 (符合 ISO 8601 格式)
              "image_base64": "...", // Base64 編碼的 JPEG 圖片數據
              "content_type": "image/jpeg"
            }
            ```
        *   **響應 (失敗)：** 返回適當的 HTTP 狀態碼 (e.g., `500 Internal Server Error`) 和包含錯誤訊息的 JSON 響應。
    *   **端點：** `POST http://[ESP32_CAM_IP]:[HTTP_SERVER_PORT]/api/trigger_push` (用於模式一：後端信號觸發推送)
        *   **功能：** 接收來自 Django 後端系統的 HTTP POST 請求。請求體中包含後端提供的元數據，例如 `operation_correlation_id` 和 `user_identifier`。接收到請求後，ESP32-CAM 應執行拍照流程，並**使用接收到的元數據**，將圖像數據**主動推送至配置的雲端 API Gateway 上傳端點 (參見 4.3.2)**。
        *   **請求：** HTTP POST 請求，請求體為 JSON 格式。
            ```json
            {
              "operation_correlation_id": "...", // 後端生成的關聯 ID
              "user_identifier": "..." // 後端提供的用戶標識符
            }
            ```
        *   **響應 (成功 `202 Accepted`)：** JSON 格式響應，表示觸發信號已接收並處理中。
        *   **響應 (失敗)：** 返回適當的 HTTP 狀態碼和錯誤訊息。

*   **4.3.2. ESP32 作為 HTTP 客戶端 (用於模式一：ESP32 主動推送至雲端)**
    *   **目標端點 (配置項)：** `POST https://[API_GATEWAY_ID].execute-api.[REGION].amazonaws.com/[STAGE]/upload` (或其他配置的雲端 API Gateway 上傳端點)
        *   **功能：** 在接收到內部觸發 (如門感應器) 或來自後端的 `/api/trigger_push` 請求後，ESP32-CAM 執行拍照，將 JPEG 圖像數據進行 Base64 編碼，並組合成 JSON 格式的請求體，**主動**向配置的 `AWS_UPLOAD_ENDPOINT` 發送 HTTP POST 請求。
        *   **請求：** HTTP POST 請求。
            *   **Header：**
                *   `Content-Type: application/json`
                *   `x-api-key: [AWS_API_KEY]` (使用配置好的 API 金鑰進行認證)
            *   **請求體 (JSON)：**
                ```json
                {
                  "device_id": "...", // 設備的唯一 ID (DEVICE_ID)
                  "timestamp_utc": "YYYY-MM-DDTHH:MM:SSZ", // UTC 時間戳 (符合 ISO 8601 格式)
                  "image_base64": "...", // Base64 編碼的 JPEG 圖片數據
                  "content_type": "image/jpeg", // 固定的內容類型
                  "operation_correlation_id": "...", // 如果是從後端 trigger_push 獲取，則包含
                  "user_identifier": "..." // 如果是從後端 trigger_push 獲取，則包含
                }
                ```
        *   **響應：** 接收並解析雲端 API Gateway 返回的 HTTP 狀態碼 (預期 `202 Accepted` 或 `200 OK`) 和響應體，以確認上傳是否成功。應記錄上傳結果（成功或失敗）。

#### 4.4. 模式切換與配置

*   ESP32-CAM 的固件應包含一個配置項（例如，編譯時宏定義或儲存在 NVS 中），用於指定當前運行的圖像採集模式（模式一：主動推送 或 模式二：後端拉取）。
*   根據配置的模式，啟用或禁用相應的 HTTP 服務器端點 (`/api/photos`, `/api/trigger_push`) 和 HTTP 客戶端推送邏輯。
*   不同的模式需要配置不同的後端/雲端通信參數 (例如，模式二需要後端知道 ESP32 的 IP/Port；模式一需要配置 AWS API Gateway Endpoint 和 API Key)。

### 5. 配置項

使用者需要根據實際環境和選擇的圖像採集模式修改程式碼中的以下配置項：

*   `CameraWebServer.ino`:
    *   `CAMERA_MODEL_...`: 選擇對應的攝像頭型號。
    *   `WIFI_SSID`: 您的 Wi-Fi 名稱。
    *   `WIFI_PASSWORD`: 您的 Wi-Fi 密碼。
    *   `NTP_SERVER1`, `NTP_SERVER2`: NTP 服務器地址。
    *   `TZ_INFO`: 時區訊息 (例如 `"CST-8"` 表示台灣時間)。
    *   `IMAGE_COLLECTION_MODE`: 定義當前運行的模式 (例如 `MODE_PULL` 或 `MODE_PUSH`)。
*   `arduino/config.h` (建議新增一個單獨的配置檔案):
    *   `DEVICE_ID`: 設備的唯一標識符字串 (例如 `"fridge_001"`)。
    *   `HTTP_SERVER_PORT`: HTTP 伺服器監聽的端口 (預設 81)。
    *   (若 `IMAGE_COLLECTION_MODE` 為 `MODE_PUSH`) `AWS_UPLOAD_ENDPOINT`: 您的 AWS API Gateway 上傳端點 URL。
    *   (若 `IMAGE_COLLECTION_MODE` 為 `MODE_PUSH`) `AWS_API_KEY`: 您的 AWS API Gateway 金鑰。
*   `arduino/camera_pins.h`: 包含各種型號的引腳定義，通常只需在 `CameraWebServer.ino` 中定義 `CAMERA_MODEL_...` 即可自動包含。

### 6. 使用方式

1.  使用 Arduino IDE 或 PlatformIO 開啟專案。
2.  根據硬體和環境需求以及所需的圖像採集模式修改配置項 (SSID, Password, `DEVICE_ID`, `IMAGE_COLLECTION_MODE`, 以及對應模式所需的 AWS/後端參數)。
3.  編譯並將程式碼燒錄到 ESP32-CAM 開發板。
4.  開啟串口監視器查看啟動日誌，獲取設備分配到的 IP 地址 (在模式二中後端需要此訊息)。
5.  根據配置的模式：
    *   **模式二 (後端拉取):** 確保後端系統可以通過獲取的 IP 地址和端口訪問 `/api/photos` 端點。
    *   **模式一 (ESP32 推送):** 確保 ESP32 可以訪問配置的 `AWS_UPLOAD_ENDPOINT`，並配置後端在需要時調用 `/api/trigger_push` (如果採用後端觸發) 或依賴本地觸發。
6.  (可選) 在同一區域網路下的瀏覽器中輸入 `http://<ESP32-CAM 的 IP 地址>:[HTTP_SERVER_PORT]` 來訪問 Web 介面進行測試。
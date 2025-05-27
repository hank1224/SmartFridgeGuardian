# 專案規格書：公共冰箱倉儲管理系統

**目錄：**

1.  簡介與目標
2.  使用者角色與權限
3.  系統核心功能
4.  關鍵使用者操作流程
    4.1. 系統管理員流程
    4.2. 冰箱使用者流程
5.  系統架構
    5.1. 硬體整合 (ESP32-CAM 智慧攝影機)
    5.2. 軟體架構 (Django 後端)
    5.3. 資料庫模型概覽
    5.4. 外部服務整合
6.  技術棧
7.  非功能性需求 (初步)
8.  未來擴展方向 (PoC 範圍外)

---

**1. 簡介與目標**

「公共冰箱倉儲管理系統」旨在利用物聯網 (IoT) 攝影機和人工智慧 (AI) 圖像識別技術，為共享或公共冰箱提供一個智能化的物品管理解決方案。系統允許使用者在放入物品時自動拍照並識別內容，追蹤物品的存放者和時間，並提供一個介面供使用者查詢冰箱內容和個人存放的物品。

**專案目標 (概念驗證 PoC 階段)：**

*   建立一個 Django 後端系統，能夠與 ESP32-CAM 攝影機模組整合。
*   實現使用者透過網頁介面操作冰箱（放入/取出物品），並在操作時觸發攝影機拍照。
*   將拍攝的照片上傳至雲端儲存 (AWS S3)。
*   調用大型語言模型 (LLM) API (如 OpenAI 格式的 API) 對照片內容進行分析，識別物品、數量及推測保質期。
*   將識別結果與操作用戶關聯，並存儲於資料庫。
*   提供介面供系統管理員管理冰箱設備。
*   提供介面供冰箱使用者查看冰箱內容及個人存放物品。

---

**2. 使用者角色與權限**

本系統定義了兩種主要的使用者角色：

*   **2.1. 系統管理員 (Admin):**
    *   **權限：**
        *   登入管理後台。
        *   註冊、查看、編輯和刪除冰箱設備 (ESP32-CAM) 的配置信息（如名稱、IP 地址）。
        *   查看所有冰箱的操作日誌和拍攝的照片。
        *   查看所有使用者存放的物品記錄。
        *   (未來可能) 管理使用者帳戶。
    *   **介面：** 主要通過 Django Admin 後台或特定的管理員網頁介面進行操作。

*   **2.2. 冰箱使用者 (User):**
    *   **權限：**
        *   註冊和登入系統。
        *   瀏覽可用的公共冰箱列表。
        *   選擇特定冰箱進行操作（放入物品、取出物品）。
        *   操作冰箱時，系統會自動記錄其身份並拍攝照片。
        *   查看自己放入冰箱的物品列表及其詳情（物品名稱、數量、放入時間、預估保質期）。
        *   查看特定冰箱當前（基於最新照片分析的）大致內容。
    *   **介面：** 主要通過系統的前端網頁介面進行操作。

---

**3. 系統核心功能**

*   **3.1. 使用者管理與認證：**
    *   使用者註冊、登入、登出功能。
    *   基於角色的訪問控制。

*   **3.2. 冰箱設備管理 (管理員)：**
    *   新增冰箱設備，需手動配置設備 ID (來自 ESP32-CAM)、IP 地址和端口。
    *   編輯和查看已註冊的冰箱設備信息。

*   **3.3. 圖像採集與儲存：**
    *   冰箱使用者操作冰箱（放入/取出）時，系統自動向指定的 ESP32-CAM 發送拍照指令。
    *   ESP32-CAM 拍攝照片（JPEG 格式），將圖像數據進行 Base64 編碼後連同時間戳等元數據以 JSON 格式返回給 Django 後端。
    *   Django 後端接收 Base64 數據，解碼後將圖片上傳至 AWS S3 進行持久化儲存。

*   **3.4. 圖像內容識別 (AI/LLM)：**
    *   圖片成功儲存後，系統異步調用 OpenAI 相容格式的 LLM API。
    *   向 LLM 發送圖片數據和特定提示 (Prompt)，要求其識別圖片中的物品、數量及推測保質期，並以固定的 JSON 格式返回結果。

*   **3.5. 物品庫存追蹤：**
    *   系統解析 LLM 返回的 JSON 結果。
    *   為每個識別出的物品創建記錄，包含物品名稱、數量、預估保質期、放入日期以及**物品擁有者（即操作時登入的冰箱使用者）**。
    *   所有物品記錄存儲在資料庫中，並與對應的照片和使用者關聯。

*   **3.6. 使用者操作介面：**
    *   提供清晰的網頁介面供冰箱使用者選擇冰箱、執行放入/取出操作。
    *   顯示冰箱的當前內容概覽（基於最新分析的照片）。
    *   允許使用者查看個人存放物品列表。

---

**4. 關鍵使用者操作流程**

**4.1. 系統管理員流程**

*   **4.1.1. 註冊新冰箱設備：**
    1.  管理員登入系統管理介面。
    2.  導航至冰箱設備管理頁面。
    3.  點擊 "新增冰箱" 按鈕。
    4.  填寫冰箱表單：
        *   冰箱名稱 (e.g., "三樓茶水間冰箱")
        *   設備 ID (ESP32-CAM 內部設定的 ID, e.g., "fridge_1")
        *   ESP32-CAM 的 IP 地址 (手動配置)
        *   ESP32-CAM 的 HTTP 服務端口 (預設 `81`)
        *   位置描述 (可選)
    5.  提交表單，系統保存冰箱設備資訊。

**4.2. 冰箱使用者流程**

*   **4.2.1. 登入系統：**
    1.  使用者訪問系統登入頁面。
    2.  輸入帳號密碼進行登入。

*   **4.2.2. 放入物品到冰箱：**
    1.  **選擇冰箱：**
        *   登入後，使用者看到可用冰箱列表。
        *   使用者點擊選擇一台目標冰箱。
    2.  **選擇操作：**
        *   系統顯示所選冰箱的資訊。
        *   使用者點擊 "**放入物品**" 按鈕。
    3.  **確認並"開啟"冰箱 (觸發拍照)：**
        *   使用者可能會看到一個操作確認提示。
        *   使用者點擊（虛擬的）"**開啟冰箱並拍照**" 按鈕。
        *   前端向 Django 後端發送請求，包含所選 `fridge_id` 和 `operation_type='put_in'`。
    4.  **後端處理與拍照：**
        *   Django 後端驗證使用者身份。
        *   (可選) 記錄操作日誌 (`FridgeOperationLog`)。
        *   後端向對應 IP 地址的 ESP32-CAM 的 `/api/photos` 端點發送 `GET` 請求。
        *   ESP32-CAM 開啟閃光燈、拍照、關閉閃光燈，返回包含 Base64 編碼圖像、時間戳等的 JSON 數據。
    5.  **圖像處理與儲存：**
        *   Django 後端接收到 JSON 數據。
        *   若成功：
            *   解碼 Base64 圖像數據。
            *   創建 `Photo` 記錄，關聯 `FridgeDevice` 和操作使用者 (`uploaded_by = request.user`)。
            *   將圖像上傳至 AWS S3。
            *   `Photo` 記錄的 `recognition_status` 設為 'pending'。
            *   保存 `Photo` 記錄。
            *   向前端返回操作已記錄、正在處理的訊息。
        *   若失敗（ESP32-CAM 無響應或返回錯誤）：
            *   記錄錯誤，向前端返回失敗訊息。
    6.  **使用者實際操作：**
        *   使用者實際將物品放入冰箱。
        *   使用者關閉網頁或離開該頁面（象徵冰箱門關閉）。
    7.  **異步圖像分析 (Celery Task)：**
        *   一個異步任務被觸發，處理該 `Photo` 記錄。
        *   任務將 `Photo.recognition_status` 更新為 'processing'。
        *   任務從 S3 獲取圖片，調用 `ImageRecognitionService` 將圖片發送給 LLM API 進行分析。
        *   LLM API 返回分析結果 (JSON 格式的物品列表)。
        *   任務解析結果，為每個識別出的物品創建 `RecognizedItem` 記錄：
            *   `photo` = 關聯的 `Photo` 實例。
            *   `name`, `quantity`, `estimated_expiry_info` = 來自 LLM。
            *   `placement_date` = 物品放入日期 (基於照片時間戳)。
            *   `owner` = `Photo.uploaded_by` (操作冰箱的登入使用者)。
        *   保存 `RecognizedItem` 記錄。
        *   更新 `Photo.recognition_status` 為 'completed' (或 'failed' 若分析失敗)。
    8.  **查看結果：**
        *   使用者後續可以在 "我的物品" 頁面或冰箱內容頁面看到新放入並被識別的物品。

*   **4.2.3. 取出物品從冰箱 (PoC 簡化流程)：**
    1.  **選擇冰箱與操作：** 同 "放入物品" 流程的步驟 1 和 2，但使用者點擊 "**取出物品**" 按鈕。
    2.  **確認並"開啟"冰箱 (觸發拍照)：** 同 "放入物品" 流程的步驟 3，`operation_type='take_out'`。
    3.  **後端處理與拍照：** 同 "放入物品" 流程的步驟 4。
    4.  **圖像處理與儲存：** 同 "放入物品" 流程的步驟 5。新照片仍會被拍攝並儲存，`uploaded_by` 記錄操作者。
    5.  **使用者實際操作：** 使用者實際從冰箱取出物品。
    6.  **異步圖像分析 (Celery Task)：**
        *   同 "放入物品" 流程的步驟 7。系統仍會分析新照片中的物品。
        *   **PoC 注意：** 此階段，"取出物品" 操作主要目的是記錄一次冰箱門開啟事件和當時的快照。系統**不會**自動比較照片來判斷哪些物品被取出。識別出的物品仍會以新記錄（如果 LLM 依然識別到它們）的形式出現，並歸屬於操作者（這點在 "取出" 場景下可能不完全符合直覺，但這是 PoC 的簡化）。
    7.  **查看結果：** 使用者操作被記錄。對庫存的實際影響追蹤是未來可優化的點。

*   **4.2.4. 查看個人存放的物品：**
    1.  使用者登入後，導航至 "我的物品" 頁面。
    2.  系統顯示一個列表，包含該使用者 `owner` 的所有 `RecognizedItem`，可按冰箱分組或篩選。

*   **4.2.5. 查看冰箱當前內容：**
    1.  使用者選擇一台冰箱。
    2.  系統展示該冰箱最新一張 `Photo` (且 `recognition_status='completed'`) 所分析出的 `RecognizedItem` 列表，作為冰箱當前內容的概覽。

---

**5. 系統架構**

**5.1. 硬體整合 (ESP32-CAM 智慧攝影機)**

*   **裝置：** ESP32-CAM AI-Thinker 模組 (核心晶片 ESP32-WROVER-B, OV2640 攝影機)。
*   **功能：** Wi-Fi 連接，內建 HTTP 伺服器。
*   **網路：** DHCP 獲取 IP，Django 後端需知道此 IP。HTTP 伺服器監聽端口 `81`。
*   **API 接口 (由 ESP32-CAM 提供給 Django 調用)：**
    *   **端點：** `GET http://[ESP32_CAM_IP]:81/api/photos`
    *   **請求：** 無參數。
    *   **響應 (成功 `200 OK`)：**
        ```json
        {
          "id": "fridge_1", // 設備 ID
          "timestamp": "2025-05-26T18:47:37Z", // UTC 時間戳 (ISO 8601)
          "image_base64": "/9j/4AAQSkZJRgABAQAAAQA...", // Base64 編碼的 JPEG 圖像數據
          "content_type": "image/jpeg" // 圖像 MIME 類型
        }
        ```
    *   **響應 (失敗 `503 Service Unavailable`)：**
        ```json
        {
          "status": "error",
          "message": "Failed to get camera frame"
        }
        ```
*   **拍照觸發：** 由 Django 後端主動發送 GET 請求觸發（拉取模型）。

**5.2. 軟體架構 (Django 後端)**

*   **框架：** Django
*   **建議 App 結構：**
    *   `project_name/`: 專案配置 (`settings.py`, `urls.py`, `celery.py`)
    *   `apps/core/`: 核心功能、基礎模板、通用工具。
    *   `apps/users/`: 使用者認證 (使用 Django 內建 User 模型)、個人資料。
    *   `apps/fridges/`: 管理冰箱設備 (`FridgeDevice` 模型)、處理使用者選擇冰箱和操作的視圖。
    *   `apps/photos/`: 儲存照片元數據 (`Photo` 模型)、處理圖片上傳至 S3。
    *   `apps/inventory/`: 儲存和管理 LLM 辨識出的物品 (`RecognizedItem` 模型)、提供物品查詢視圖。
*   **關鍵服務層：**
    *   `ESP32CamService`: 封裝與 ESP32-CAM API 的通訊邏輯。
    *   `ImageRecognitionService`: 封裝與 LLM API 的通訊、Prompt 構造和結果解析邏輯。
*   **異步任務：** 使用 Celery (配合 Redis 或 RabbitMQ 作為 Broker) 處理耗時的圖像分析任務，避免阻塞使用者請求。

**5.3. 資料庫模型概覽 (主要模型)**

*   **`User` (來自 `django.contrib.auth.models`)**: 存儲使用者帳號資訊。
*   **`FridgeDevice` (`fridges` app)**:
    *   `name` (CharField), `device_id_esp` (CharField, unique), `ip_address` (GenericIPAddressField), `port` (IntegerField), `location_description` (TextField), `is_active` (BooleanField), `created_at`, `updated_at`.
*   **`Photo` (`photos` app)**:
    *   `fridge_device` (ForeignKey to `FridgeDevice`), `image` (ImageField, upload_to S3), `timestamp_esp` (DateTimeField), `content_type_esp` (CharField), `uploaded_by` (ForeignKey to `User`), `uploaded_at` (DateTimeField), `recognition_status` (CharField), `raw_llm_response` (JSONField, optional).
*   **`RecognizedItem` (`inventory` app)**:
    *   `photo` (ForeignKey to `Photo`), `name` (CharField), `quantity` (CharField/IntegerField), `estimated_expiry_info` (TextField), `placement_date` (DateField), `owner` (ForeignKey to `User`), `added_at` (DateTimeField), `notes` (TextField, optional).
*   **`FridgeOperationLog` (`fridges` app - 可選但推薦)**:
    *   `user` (ForeignKey to `User`), `fridge_device` (ForeignKey to `FridgeDevice`), `operation_type` (CharField), `operation_start_time` (DateTimeField), `photo_taken` (OneToOneField to `Photo`, nullable).

**5.4. 外部服務整合**

*   **AWS S3 (Simple Storage Service)：** 用於儲存拍攝的冰箱照片。通過 `django-storages` 和 `boto3` 庫整合。
*   **OpenAI 相容格式的 LLM API：** 用於圖像內容識別。通過 `requests` 庫調用。API 金鑰等敏感資訊使用 `python-dotenv` 管理，從環境變數讀取。

---

**6. 技術棧**

*   **後端：** Python, Django
*   **資料庫：** PostgreSQL (推薦生產環境) / MySQL / SQLite (開發環境)
*   **異步任務隊列：** Celery
*   **消息中間件 (Broker for Celery)：** Redis (推薦) / RabbitMQ
*   **圖像儲存：** AWS S3
*   **環境變數管理：** `python-dotenv`
*   **HTTP 請求庫：** `requests` (用於調用 ESP32-CAM 和 LLM API)
*   **AWS SDK for Python:** `boto3` (由 `django-storages` 間接使用)
*   **Django S3 整合：** `django-storages`
*   **前端：** Django Templates, HTML, CSS, JavaScript (用於基本交互，無需複雜前端框架)

---

**7. 非功能性需求 (初步)**

*   **安全性：**
    *   使用者密碼需加密儲存。
    *   API 金鑰 (AWS, LLM) 不得硬編碼，應通過環境變數配置。
    *   對使用者輸入進行適當的驗證和清理。
    *   實施基於角色的授權。
*   **易用性：**
    *   為管理員和冰箱使用者提供清晰、直觀的操作介面。
    *   對異步處理（如圖像分析）提供明確的狀態反饋。
*   **可靠性：**
    *   對與 ESP32-CAM 和 LLM API 的通訊進行錯誤處理和重試機制（初步）。
    *   詳細的日誌記錄，便於問題排查。
*   **性能：**
    *   異步處理圖像分析，避免阻塞使用者。
    *   資料庫查詢優化。
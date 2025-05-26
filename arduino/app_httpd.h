// app_httpd.h

#ifndef APP_HTTPD_H
#define APP_HTTPD_H

#include <esp_http_server.h> // 需要 esp_http_server 庫來定義 httpd_handle_t

// HTTP 伺服器監聽端口
const int HTTP_SERVER_PORT = 81;

// 攝像頭自動拍照任務的頻率 (毫秒) - 此常數在修改後已不再使用，但為兼容性可保留或移除。
// const unsigned long AUTO_CAPTURE_INTERVAL_MS = 5000; // 每 5 秒

/**
 * @brief 啟動攝像頭的 HTTP 伺服器。
 *        包含實時串流 (/stream) 和單張圖片捕獲 (/capture) 服務。
 *        同時啟動一個獨立任務進行定時自動拍照。
 */
void startCameraServer();

/**
 * @brief 初始化 LED 閃光燈引腳。
 *        將引腳設置為輸出模式並確保其初始為關閉狀態。
 * @param pin LED 連接的 GPIO 引腳號。
 */
void setupLedFlash(int pin);

#endif // APP_HTTPD_H
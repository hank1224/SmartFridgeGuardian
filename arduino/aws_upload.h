// aws_upload.h

#ifndef AWS_UPLOAD_H
#define AWS_UPLOAD_H

#include <esp_camera.h> // 需要 camera_fb_t 結構體

/**
 * @brief 執行 AWS 相關的初始化設定。
 *        例如：AWS SDK 客戶端初始化、證書加載等。
 *        此函數應在 setup() 中調用一次。
 */
void aws_upload_setup();

/**
 * @brief 將攝像頭拍攝到的照片幀緩衝區上傳至 AWS。
 *        這是一個預留接口，您將在這裡實現實際的 AWS 上傳邏輯。
 * @param fb 指向 camera_fb_t 結構體的指針，包含圖像數據。
 * @return bool 返回 true 表示上傳操作成功，false 表示失敗。
 */
bool uploadPhotoToAWS(camera_fb_t *fb);

#endif // AWS_UPLOAD_H
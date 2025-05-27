import base64
import json
import logging
import re

import requests

from .models import FridgeDevice

logger = logging.getLogger(__name__)

class ESP32CamService:
    @staticmethod
    def _fix_malformed_json(json_str: str) -> str:
        """
        修復格式不正確的 JSON 字符串，主要是處理屬性名沒有引號的情況

        Args:
            json_str: 原始 JSON 字符串

        Returns:
            str: 修復後的 JSON 字符串
        """
        # 使用正則表達式匹配沒有引號的屬性名
        pattern = r'([{,])\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*:'

        def replace_property(match):
            prefix = match.group(1)  # 匹配到的 { 或 ,
            prop_name = match.group(2)  # 匹配到的屬性名
            return f'{prefix} "{prop_name}":'

        # 替換所有沒有引號的屬性名
        fixed_json = re.sub(pattern, replace_property, json_str)
        return fixed_json

    @staticmethod
    def fetch_photo_data(device: FridgeDevice) -> dict:
        """
        從 ESP32-CAM 獲取照片數據

        Args:
            device: FridgeDevice 實例

        Returns:
            Dict: 包含照片數據的字典，格式如下：
            {
                'id': str,  # ESP32-CAM 設備 ID
                'timestamp': str,  # 拍攝時間戳
                'image_base64': str,  # Base64 編碼的圖片數據
                'content_type': str  # 圖片 MIME 類型
            }

        Raises:
            requests.RequestException: 當請求失敗時拋出
        """
        try:
            # 記錄請求的端點
            endpoint = device.get_api_endpoint()
            logger.debug(f"Requesting ESP32-CAM endpoint: {endpoint}")

            response = requests.get(
                endpoint,
                timeout=10
            )

            # 記錄響應狀態碼和頭部
            logger.debug(f"ESP32-CAM response status: {response.status_code}")
            logger.debug(f"ESP32-CAM response headers: {dict(response.headers)}")

            # 打印原始響應內容
            logger.debug(f"ESP32-CAM raw response: {response.text}")

            try:
                # 首先嘗試直接解析 JSON
                data = response.json()
            except json.JSONDecodeError as e:
                logger.warning(f"Initial JSON parsing failed, attempting to fix malformed JSON: {str(e)}")
                try:
                    # 嘗試修復 JSON 格式
                    fixed_json = ESP32CamService._fix_malformed_json(response.text)
                    logger.debug(f"Fixed JSON: {fixed_json}")
                    data = json.loads(fixed_json)
                except (json.JSONDecodeError, Exception) as e:
                    logger.error(f"Failed to parse JSON response after fixing: {str(e)}")
                    logger.error(f"Original response content: {response.text}")
                    raise ValueError(f"ESP32-CAM 返回的 JSON 格式不正確且無法修復: {str(e)}") from e

            # 驗證返回的數據格式
            required_fields = ['id', 'timestamp', 'image_base64', 'content_type']
            missing_fields = [field for field in required_fields if field not in data]
            if missing_fields:
                raise ValueError(f"ESP32-CAM 返回的數據缺少必要字段: {', '.join(missing_fields)}")

            # 驗證設備 ID 是否匹配
            if data['id'] != device.device_id_esp:
                raise ValueError(f"ESP32-CAM 返回的設備 ID ({data['id']}) 與配置的 ID ({device.device_id_esp}) 不匹配")

            return data

        except requests.RequestException as e:
            # 記錄錯誤並重新拋出
            logger.error(f"ESP32-CAM request failed: {str(e)}")
            raise requests.RequestException(f"從 ESP32-CAM 獲取照片失敗: {str(e)}") from e
        except ValueError as e:
            # 記錄 JSON 解析錯誤
            logger.error(f"ESP32-CAM data validation error: {str(e)}")
            raise

    @staticmethod
    def decode_base64_image(image_base64: str) -> bytes:
        """
        解碼 Base64 圖片數據

        Args:
            image_base64: Base64 編碼的圖片數據
        Returns:
            bytes: 解碼後的圖片二進制數據
        """
        try:
            return base64.b64decode(image_base64)
        except Exception as e:
            logger.error(f"Base64 decode failed: {str(e)}")
            raise ValueError(f"Base64 解碼失敗: {str(e)}") from e

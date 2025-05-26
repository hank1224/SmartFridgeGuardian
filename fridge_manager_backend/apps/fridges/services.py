import requests
import base64
from typing import Dict, Optional
from .models import FridgeDevice

class ESP32CamService:
    @staticmethod
    def fetch_photo_data(device: FridgeDevice) -> Dict:
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
            response = requests.get(
                f'http://{device.ip_address}:{device.port}/api/photos',
                timeout=10
            )
            response.raise_for_status()
            
            data = response.json()
            
            # 驗證返回的數據格式
            required_fields = ['id', 'timestamp', 'image_base64', 'content_type']
            if not all(field in data for field in required_fields):
                raise ValueError("ESP32-CAM 返回的數據格式不正確")
                
            # 驗證設備 ID 是否匹配
            if data['id'] != device.device_id_esp:
                raise ValueError("ESP32-CAM 返回的設備 ID 不匹配")
                
            return data
            
        except requests.RequestException as e:
            # 記錄錯誤並重新拋出
            raise requests.RequestException(f"從 ESP32-CAM 獲取照片失敗: {str(e)}")
            
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
            raise ValueError(f"Base64 解碼失敗: {str(e)}")
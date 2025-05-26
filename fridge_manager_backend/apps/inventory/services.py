import json
import requests
from typing import List, Dict
from django.conf import settings
from apps.photos.models import Photo

class ImageRecognitionService:
    @staticmethod
    def analyze_image_with_llm(image_file_path: str, photo_instance: Photo) -> List[Dict]:
        """
        使用 LLM API 分析圖片
        
        Args:
            image_file_path: 圖片文件路徑
            photo_instance: Photo 實例
            
        Returns:
            List[Dict]: 識別出的物品列表，每個物品包含：
            {
                'name': str,  # 物品名稱
                'quantity': str,  # 數量描述
                'estimated_expiry_info': str  # 預估保質期信息
            }
        """
        try:
            # 讀取圖片文件
            with open(image_file_path, 'rb') as f:
                image_data = f.read()
            
            # 準備發送給 LLM API 的請求
            headers = {
                'Authorization': f'Bearer {settings.OPENAI_API_KEY}',
                'Content-Type': 'application/json'
            }
            
            # 構建 prompt
            prompt = """
            請分析這張冰箱內部的圖片。識別圖片中的所有食品和飲料。
            對於每件物品，請提供以下JSON格式的資訊：
            {
              "name": "物品名稱 (例如：可口可樂 330ml, 青蘋果)",
              "quantity": "數量描述 (例如：1 瓶, 3 顆, 一袋)",
              "estimated_expiry_info": "根據物品類型和常見情況，推測的保質期或建議存放時長 (例如：建議一週內飲用完畢, 可冷藏2-3週, 已過期)。如果無法判斷，請填寫'未知'或'無法推測'"
            }
            請將所有識別出的物品以JSON數組的形式返回，數組鍵名為 'recognized_items'。
            例如： { "recognized_items": [ { "name": "...", ... }, { "name": "...", ... } ] }
            如果無法識別任何物品，請返回空的 'recognized_items' 數組。
            """
            
            # 發送請求到 LLM API
            response = requests.post(
                'https://api.openai.com/v1/chat/completions',
                headers=headers,
                json={
                    'model': 'gpt-4-vision-preview',
                    'messages': [
                        {
                            'role': 'user',
                            'content': [
                                {'type': 'text', 'text': prompt},
                                {
                                    'type': 'image_url',
                                    'image_url': {
                                        'url': f'data:image/jpeg;base64,{image_data}'
                                    }
                                }
                            ]
                        }
                    ],
                    'max_tokens': 1000
                }
            )
            
            response.raise_for_status()
            result = response.json()
            
            # 解析 LLM 返回的 JSON
            try:
                content = result['choices'][0]['message']['content']
                parsed_content = json.loads(content)
                return parsed_content.get('recognized_items', [])
            except (KeyError, json.JSONDecodeError) as e:
                raise ValueError(f"解析 LLM 返回的數據失敗: {str(e)}")
                
        except requests.RequestException as e:
            raise requests.RequestException(f"LLM API 請求失敗: {str(e)}")
        except Exception as e:
            raise Exception(f"圖像分析過程中發生錯誤: {str(e)}")
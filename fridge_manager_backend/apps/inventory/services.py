import base64
import json
import logging

from django.conf import settings
from openai import OpenAI

from apps.photos.models import Photo

logger = logging.getLogger(__name__)

# Constants
CONTENT_PREVIEW_LENGTH = 500

class ImageRecognitionService:
    @staticmethod
    def analyze_image_with_llm(image_file_path: str, photo_instance: Photo) -> list[dict]:
        """
        使用 LM Studio API 分析圖片

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
            logger.info(f"開始分析圖片: {image_file_path}")

            # 讀取圖片文件並進行 Base64 編碼
            with open(image_file_path, 'rb') as f:
                image_data = f.read()
                base64_image = base64.b64encode(image_data).decode('utf-8')
                # 假設圖片格式為 JPEG，如果可能有多種格式，需要更複雜的處理
                base64_image_url = f"data:image/jpeg;base64,{base64_image}"

            # 初始化 OpenAI 客戶端，指向本地 LM Studio API
            client = OpenAI(
                base_url=settings.LMSTUDIO_API_URL,  # 例如: "http://localhost:1234/v1"
                api_key="lm-studio"  # LM Studio 不需要真正的 API Key
            )

            prompt = """
你是一個專門食品圖片並推估保存期限的AI助手。

當接收到一張圖片時，請執行以下分析並生成 JSON 輸出：

1.  **掃描與識別：** 仔細掃描圖片，識別出食品或飲料的個別物品或狀態一致的組。
2.  **詳細分析：** 請根據其具體類型、可見的狀態（如：數量、包裝是否完整、外觀是否有異）以及你對該類食品普遍保存知識的理解，盡最大努力、詳細且具體地推測其保質期、建議的存放時長。
3.  **生成 JSON 輸出：** 將所有識別出的物品/組以以下指定的 JSON 格式返回。

**JSON 格式要求：**

```json
{
  "recognized_items": [
    {
      "name": "物品名稱 (請盡可能具體，例如：青蘋果 (大), 青蘋果 (小), 已開封的可口可樂 330ml, 整盒未開封的雞蛋)",
      "quantity": "數量描述 (例如：1 顆, 5 顆, 1 瓶, 3 瓶, 1 盒 (10個))",
      "estimated_expiry_info": "請針對此物品推測一個清晰的時間範圍或狀態描述。範例：'一週內', '三天內', '2-3週’”
    }
  ]
}
```

**`estimated_expiry_info` 的嚴格要求：**

*   請直接提供一個**時間範圍**（如「一週內」、「2-3週」、「2個月」）
*   請避免主觀判斷（如「看起來很新鮮」）或建議食用完整句子（如「建議一週內食用」）的表達方式。

**其他要求：**

*   `name` 應盡可能具體，如果同類物品有不同狀態（如大小、是否開封、品牌差異），請在名稱中區分。
*   `quantity` 請精確描述該個體或組的數量。
*   如果無法識別任何物品，請返回一個空的 `recognized_items` 數組：`{ "recognized_items": [] }`。

請嚴格遵守上述所有要求和 JSON 格式進行分析和返回。
            """

            # 發送請求到 LM Studio API
            logger.info(f"正在向 LLM API 發送請求: {settings.LMSTUDIO_API_URL}/chat/completions")
            response = client.chat.completions.create(
                model=settings.LMSTUDIO_MODEL_NAME,  # 在 settings.py 中配置的模型名稱
                messages=[
                    {
                        "role": "user",
                        "content": [
                            {"type": "text", "text": prompt},
                            {
                                "type": "image_url",
                                "image_url": {
                                    "url": base64_image_url,
                                    "detail": "auto"
                                }
                            }
                        ]
                    }
                ],
                max_tokens=1024 # 保持 max_tokens 不變或適當調整
            )
            logger.info("成功接收到 LLM API 響應")


            # 解析 LLM 返回的內容
            try:
                # 從響應中獲取模型的文字內容
                content = response.choices[0].message.content
                logger.debug(f"LLM 返回的原始 content:\n{content}") # 打印原始返回內容供除錯

                # --- 新增邏輯：處理模型可能返回的 markdown 代碼塊包裹的 JSON ---
                json_string_to_parse = content

                # 尋找 markdown JSON 代碼塊的標記
                json_start_marker = '```json\n'
                json_end_marker = '\n```' # 根據之前的測試輸出調整結束標記，注意換行符

                start_index = content.find(json_start_marker)
                end_index = content.rfind(json_end_marker) # 使用 rfind 尋找最後一個結束標記

                if start_index != -1 and end_index != -1 and end_index > start_index:
                    # 如果找到了標記，提取中間的部分作為 JSON 字串
                    # start_index + len(json_start_marker) 是 JSON 內容的實際開始位置
                    json_string_to_parse = content[start_index + len(json_start_marker):end_index].strip()
                    logger.debug("從 markdown 代碼塊中提取 JSON 成功")
                else:
                    # 如果沒有找到標記，假設整個 content 字串就是 JSON
                    # 或者模型可能返回了非預期格式
                    logger.warning("LLM 返回內容未包含預期的 '```json\\n' 和 '\\n```' 標記。嘗試直接解析原始內容。")
                    json_string_to_parse = content.strip() # 移除首尾空白字元

                logger.debug(f"準備解析的 JSON 字串:\n{json_string_to_parse}")

                # 使用提取出的（或原始的）字串進行 JSON 解析
                parsed_content = json.loads(json_string_to_parse)
                logger.info("成功解析 LLM 返回的 JSON 內容")

                # 獲取 'recognized_items' 列表，如果 key 不存在則返回空列表
                recognized_items = parsed_content.get('recognized_items', [])
                logger.info(f"識別出的物品數量: {len(recognized_items)}")

                return recognized_items

            except (KeyError, json.JSONDecodeError) as e:
                # 捕獲解析 JSON 或提取 key 時的錯誤
                error_message = f"解析 LLM 返回的數據失敗: {str(e)}"
                # 在錯誤信息中包含部分原始 content 內容，以便除錯
                raw_content_preview = content[:CONTENT_PREVIEW_LENGTH] + ('...' if len(content) > CONTENT_PREVIEW_LENGTH else '') if 'content' in locals() else 'N/A'
                logger.error(f"{error_message}. 原始LLM內容開頭: '{raw_content_preview}'", exc_info=True)
                raise ValueError(f"{error_message}. 請檢查 LLM 返回內容是否符合預期格式。") from e
            except IndexError as e:
                 # 捕獲 choices[0] 或 message 為空的情況
                 logger.error(f"從 LLM 響應中提取 content 失敗，響應結構不符合預期: {str(e)}", exc_info=True)
                 # 可以選擇打印或記錄完整的響應對象結構來除錯
                 # logger.debug(f"完整 LLM 響應對象: {response.model_dump_json(indent=2)}")
                 raise ValueError(f"從 LLM 響應中提取 content 失敗，響應結構不符合預期: {str(e)}") from e


        except Exception as e:
            # 捕獲其他可能發生的錯誤 (例如網路錯誤)
            logger.error(f"圖像分析過程中發生錯誤: {str(e)}", exc_info=True)
            raise Exception(f"圖像分析過程中發生錯誤: {str(e)}") from e

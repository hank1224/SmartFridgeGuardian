import os
import tempfile

from celery import shared_task

from apps.inventory.models import RecognizedItem
from apps.inventory.services import ImageRecognitionService
from apps.photos.models import Photo


@shared_task
def process_fridge_image(photo_id: int) -> None:
    """
    處理冰箱照片的 Celery 任務

    Args:
        photo_id: Photo 實例的 ID
    """
    try:
        # 獲取 Photo 實例
        photo = Photo.objects.get(id=photo_id)

        # 更新狀態為處理中
        photo.recognition_status = 'processing'
        photo.save()

        # 創建臨時文件
        with tempfile.NamedTemporaryFile(delete=False, suffix='.jpg') as temp_file:
            # 從 S3 下載文件到臨時位置
            with photo.image.open('rb') as image_file:
                temp_file.write(image_file.read())
            temp_file_path = temp_file.name

        try:
            # 調用 LLM 分析圖片
            recognized_items = ImageRecognitionService.analyze_image_with_llm(
                temp_file_path,
                photo
            )

            # 創建 RecognizedItem 實例
            items_to_create = []
            for item_data in recognized_items:
                items_to_create.append(
                    RecognizedItem(
                        photo=photo,
                        name=item_data['name'],
                        quantity=item_data['quantity'],
                        estimated_expiry_info=item_data['estimated_expiry_info'],
                        placement_date=photo.uploaded_at.date(),
                        owner=photo.uploaded_by
                    )
                )

            # 批量創建物品
            if items_to_create:
                RecognizedItem.objects.bulk_create(items_to_create)

            # 更新照片狀態為已完成
            photo.recognition_status = 'completed'
            photo.save()

        finally:
            # 清理臨時文件
            if os.path.exists(temp_file_path):
                os.unlink(temp_file_path)

    except Photo.DoesNotExist:
        # 如果照片不存在，記錄錯誤
        print(f"照片 ID {photo_id} 不存在")
    except Exception:
        # 如果處理過程中發生錯誤，更新照片狀態為失敗
        try:
            photo = Photo.objects.get(id=photo_id)
            photo.recognition_status = 'failed'
            photo.save()
        except Photo.DoesNotExist:
            pass
        # 重新拋出異常，讓 Celery 記錄錯誤
        raise

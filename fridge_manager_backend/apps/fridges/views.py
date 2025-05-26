from django.shortcuts import render, get_object_or_404
from django.contrib.auth.decorators import login_required
from django.http import JsonResponse
from django.views.decorators.http import require_http_methods
from .models import FridgeDevice
from .services import ESP32CamService
from apps.inventory.tasks import process_fridge_image
from apps.photos.models import Photo
from django.utils import timezone

@login_required
def fridge_list(request):
    """
    顯示所有冰箱設備的列表視圖
    """
    fridges = FridgeDevice.objects.all()
    return render(request, 'fridges/list.html', {'fridges': fridges})

@login_required
@require_http_methods(["POST"])
def trigger_photo_capture(request, device_id):
    """
    觸發 ESP32-CAM 拍照的視圖
    
    Args:
        request: HTTP 請求
        device_id: 冰箱設備 ID
        
    Returns:
        JsonResponse: 包含操作結果的 JSON 響應
    """
    try:
        # 獲取冰箱設備
        device = get_object_or_404(FridgeDevice, device_id_esp=device_id)
        
        # 從 ESP32-CAM 獲取照片數據
        photo_data = ESP32CamService.fetch_photo_data(device)
        
        # 解碼 Base64 圖片數據
        image_data = ESP32CamService.decode_base64_image(photo_data['image_base64'])
        
        # 創建 Photo 實例
        photo = Photo.objects.create(
            fridge_device=device,
            image=image_data,
            timestamp_esp=timezone.datetime.fromisoformat(photo_data['timestamp']),
            content_type_esp=photo_data['content_type'],
            uploaded_by=request.user,
            recognition_status='pending'
        )
        
        # 觸發異步任務處理圖片
        process_fridge_image.delay(photo.id)
        
        return JsonResponse({
            'status': 'success',
            'message': '照片已成功獲取，正在進行分析',
            'photo_id': photo.id
        })
        
    except Exception as e:
        return JsonResponse({
            'status': 'error',
            'message': str(e)
        }, status=500)

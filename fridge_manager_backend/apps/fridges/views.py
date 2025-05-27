import logging
import os
import tempfile
import uuid

from django.conf import settings
from django.contrib.auth.decorators import login_required
from django.contrib.auth.mixins import LoginRequiredMixin
from django.core.files import File
from django.core.files.base import ContentFile
from django.http import JsonResponse
from django.shortcuts import get_object_or_404, render
from django.utils import timezone
from django.views.decorators.http import require_http_methods
from django.views.generic import DetailView, ListView, View

from apps.inventory.tasks import process_fridge_image
from apps.photos.models import Photo

from .models import FridgeDevice, FridgeOperationLog
from .services import ESP32CamService

# 獲取 logger 實例
logger = logging.getLogger(__name__)

class UserSelectFridgeView(LoginRequiredMixin, ListView):
    """
    用戶選擇冰箱的視圖
    """
    model = FridgeDevice
    template_name = 'fridges/fridge_list_user.html'
    context_object_name = 'fridges'

    def get_queryset(self):
        return FridgeDevice.objects.filter(is_active=True)

class UserSelectOperationView(LoginRequiredMixin, DetailView):
    """
    用戶選擇操作的視圖
    """
    model = FridgeDevice
    template_name = 'fridges/fridge_select_operation.html'
    context_object_name = 'fridge'
    slug_field = 'device_id_esp'
    slug_url_kwarg = 'device_id'

    def get_queryset(self):
        return FridgeDevice.objects.filter(is_active=True)

class UserOpenFridgeView(LoginRequiredMixin, View):
    """
    用戶開啟冰箱的視圖
    """
    def _log_aws_settings(self):
        """Log AWS configuration settings."""
        logger.debug(f"DEBUG mode: {settings.DEBUG}")
        logger.debug(f"DEFAULT_FILE_STORAGE: {settings.DEFAULT_FILE_STORAGE}")
        logger.debug(f"AWS_ACCESS_KEY_ID: {os.getenv('AWS_ACCESS_KEY_ID')}")
        logger.debug(f"AWS_STORAGE_BUCKET_NAME: {os.getenv('AWS_STORAGE_BUCKET_NAME')}")
        logger.debug(f"AWS_S3_REGION_NAME: {os.getenv('AWS_S3_REGION_NAME')}")

    def _create_photo(self, device, image_data, photo_data, user):
        """Create a Photo instance from image data."""
        timestamp = timezone.now().strftime('%Y%m%d_%H%M%S')
        unique_filename = f"user_{user.id}_{timestamp}_{uuid.uuid4().hex[:8]}.jpg"

        # Create a temporary file in memory instead of on disk
        django_file = ContentFile(image_data, name=unique_filename)

        try:
            photo = Photo.objects.create(
                fridge_device=device,
                image=django_file,
                timestamp_esp=timezone.datetime.fromisoformat(photo_data['timestamp']),
                content_type_esp=photo_data['content_type'],
                uploaded_by=user,
                uploaded_at=timezone.now(),
                recognition_status='pending'
            )
            logger.info(f"Photo 實例創建成功: id={photo.id}")
            return photo
        except Exception as e:
            logger.error(f"創建 Photo 實例時發生錯誤: {str(e)}", exc_info=True)
            raise

    def post(self, request, device_id):
        """Handle fridge opening and photo capture."""
        self._log_aws_settings()

        try:
            logger.info(f"用戶 {request.user} 嘗試開啟冰箱 {device_id}")
            device = get_object_or_404(FridgeDevice, device_id_esp=device_id)
            logger.debug(f"找到冰箱設備: {device.name}")

            operation_type = request.POST.get('operation_type', 'put_in')
            if operation_type not in dict(FridgeOperationLog.OPERATION_TYPE_CHOICES):
                logger.warning(f"無效的操作類型: {operation_type}")
                return JsonResponse({'status': 'error', 'message': '無效的操作類型'}, status=400)

            operation_log = FridgeOperationLog.objects.create(
                user=request.user,
                fridge_device=device,
                operation_type=operation_type,
                notes="用戶點擊開啟冰箱按鈕"
            )
            logger.info(f"創建操作記錄: id={operation_log.id}")

            photo_data = ESP32CamService.fetch_photo_data(device)
            image_data = ESP32CamService.decode_base64_image(photo_data['image_base64'])

            photo = self._create_photo(device, image_data, photo_data, request.user)
            logger.debug(f"Photo image storage backend: {photo.image.storage.__class__.__name__}")
            logger.debug(f"Photo image URL: {photo.image.url}")

            operation_log.photo_taken = photo
            operation_log.save()
            process_fridge_image.delay(photo.id)

            return JsonResponse({
                'status': 'success',
                'message': '操作已記錄，照片已拍攝並正在處理中。您可以關閉此頁面。',
                'photo_id': photo.id
            })

        except Exception as e:
            logger.error(f"處理冰箱操作時發生錯誤: {str(e)}", exc_info=True)
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)

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

        # 生成唯一的文件名
        timestamp = timezone.now().strftime('%Y%m%d_%H%M%S')
        unique_filename = f"user_{request.user.id}_{timestamp}_{uuid.uuid4().hex[:8]}.jpg"
        temp_file_path = os.path.join(tempfile.gettempdir(), unique_filename)

        # 創建臨時文件
        with open(temp_file_path, 'wb') as temp_file:
            temp_file.write(image_data)
            temp_file.flush()

            # 創建 Photo 實例
            with open(temp_file_path, 'rb') as image_file:
                django_file = File(image_file, name=unique_filename)
                photo = Photo.objects.create(
                    fridge_device=device,
                    image=django_file,
                    timestamp_esp=timezone.datetime.fromisoformat(photo_data['timestamp']),
                    content_type_esp=photo_data['content_type'],
                    uploaded_by=request.user,
                    uploaded_at=timezone.now(),
                    recognition_status='pending'
                )

        # 刪除臨時文件
        os.unlink(temp_file_path)

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

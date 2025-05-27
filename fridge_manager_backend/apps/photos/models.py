import logging

from django.conf import settings
from django.db import models
from storages.backends.s3boto3 import S3Boto3Storage

from apps.fridges.models import FridgeDevice

# 獲取 logger 實例
logger = logging.getLogger(__name__)

class Photo(models.Model):
    RECOGNITION_STATUS_CHOICES = [
        ('pending', '等待處理'),
        ('processing', '處理中'),
        ('completed', '已完成'),
        ('failed', '失敗'),
    ]

    fridge_device = models.ForeignKey(
        FridgeDevice,
        on_delete=models.CASCADE,
        related_name='photos',
        help_text="拍攝此照片的冰箱設備"
    )
    image = models.ImageField(
        upload_to='fridge_photos/%Y/%m/%d/',
        storage=S3Boto3Storage(
            bucket_name=settings.AWS_STORAGE_BUCKET_NAME,
            region_name=settings.AWS_S3_REGION_NAME,
            custom_domain=settings.AWS_S3_CUSTOM_DOMAIN,
            querystring_auth=True,
            signature_version='s3v4',
        ),
        help_text="照片文件"
    )
    timestamp_esp = models.DateTimeField(help_text="ESP32-CAM 拍攝時間")
    content_type_esp = models.CharField(max_length=50, default='image/jpeg', help_text="照片 MIME 類型")
    uploaded_by = models.ForeignKey(
        settings.AUTH_USER_MODEL,
        on_delete=models.SET_NULL,
        null=True,
        related_name='uploaded_photos',
        help_text="觸發拍照的用戶"
    )
    uploaded_at = models.DateTimeField(auto_now_add=True, help_text="照片上傳到系統的時間")
    recognition_status = models.CharField(
        max_length=20,
        choices=RECOGNITION_STATUS_CHOICES,
        default='pending',
        help_text="LLM 辨識狀態"
    )
    raw_llm_response = models.JSONField(null=True, blank=True, help_text="LLM 原始回覆")

    class Meta:
        verbose_name = "冰箱照片"
        verbose_name_plural = "冰箱照片"
        ordering = ['-uploaded_at']

    def __str__(self):
        return f"照片 {self.id} - {self.fridge_device.name} ({self.uploaded_at})"

    def save(self, *args, **kwargs):
        try:
            logger.info(f"開始保存照片記錄: device={self.fridge_device.name}, user={self.uploaded_by}")
            if self.image:
                logger.debug(f"照片文件信息: name={self.image.name}, size={self.image.size if hasattr(self.image, 'size') else 'unknown'}")
                logger.debug(f"照片文件 URL: {self.image.url if hasattr(self.image, 'url') else 'unknown'}")
                logger.debug(f"照片文件 storage: {self.image.storage.__class__.__name__}")
            super().save(*args, **kwargs)
            logger.info(f"照片記錄保存成功: id={self.id}")
        except Exception as e:
            logger.error(f"保存照片記錄時發生錯誤: {str(e)}", exc_info=True)
            raise

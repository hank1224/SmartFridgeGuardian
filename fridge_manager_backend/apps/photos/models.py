from django.db import models
from django.conf import settings
from apps.fridges.models import FridgeDevice

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
    image = models.ImageField(upload_to='fridge_photos/%Y/%m/%d/', help_text="照片文件")
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

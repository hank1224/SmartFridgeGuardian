from django.conf import settings
from django.db import models


class FridgeDevice(models.Model):
    name = models.CharField(max_length=100, help_text="冰箱名稱，例如：一樓茶水間冰箱")
    device_id_esp = models.CharField(max_length=50, unique=True, help_text="ESP32-CAM 的設備 ID")
    api_url = models.URLField(
        help_text="ESP32-CAM 的完整 API 地址 (例如: http://192.168.1.100:81 或 http://example.com:81)",
        null=True,
        blank=True
    )
    location_description = models.TextField(blank=True, help_text="冰箱位置描述")
    is_active = models.BooleanField(default=True, help_text="設備是否啟用")
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        verbose_name = "冰箱設備"
        verbose_name_plural = "冰箱設備"

    def __str__(self):
        return f"{self.name} ({self.device_id_esp})"

    def get_api_endpoint(self) -> str:
        """
        獲取 API 端點 URL

        Returns:
            str: 完整的 API 端點 URL，格式為 {api_url}/api/photos
        """
        if not self.api_url:
            raise ValueError("設備未配置有效的 API 端點")
        return f'{self.api_url}/api/photos'

class FridgeOperationLog(models.Model):
    OPERATION_TYPE_CHOICES = [
        ('put_in', '放入物品'),
        ('take_out', '取出物品'),
    ]

    user = models.ForeignKey(
        settings.AUTH_USER_MODEL,
        on_delete=models.CASCADE,
        related_name='fridge_operations',
        help_text="執行操作的用戶"
    )
    fridge_device = models.ForeignKey(
        FridgeDevice,
        on_delete=models.CASCADE,
        related_name='operation_logs',
        help_text="操作的冰箱設備"
    )
    operation_type = models.CharField(
        max_length=20,
        choices=OPERATION_TYPE_CHOICES,
        help_text="操作類型"
    )
    operation_start_time = models.DateTimeField(
        auto_now_add=True,
        help_text="操作開始時間"
    )
    photo_taken = models.OneToOneField(
        'photos.Photo',  # 使用字符串引用來避免循環導入
        on_delete=models.SET_NULL,
        null=True,
        blank=True,
        related_name='operation_log',
        help_text="本次操作拍攝的照片"
    )
    notes = models.TextField(
        blank=True,
        help_text="操作備註"
    )

    class Meta:
        verbose_name = "冰箱操作記錄"
        verbose_name_plural = "冰箱操作記錄"
        ordering = ['-operation_start_time']

    def __str__(self):
        return f"{self.user.username} - {self.get_operation_type_display()} - {self.fridge_device.name} ({self.operation_start_time})"

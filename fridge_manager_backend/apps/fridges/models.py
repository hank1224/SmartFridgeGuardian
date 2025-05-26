from django.db import models
from django.utils import timezone

class FridgeDevice(models.Model):
    name = models.CharField(max_length=100, help_text="冰箱名稱，例如：一樓茶水間冰箱")
    device_id_esp = models.CharField(max_length=50, unique=True, help_text="ESP32-CAM 的設備 ID")
    ip_address = models.GenericIPAddressField(help_text="ESP32-CAM 的 IP 地址")
    port = models.IntegerField(default=81, help_text="ESP32-CAM 的端口號")
    location_description = models.TextField(blank=True, help_text="冰箱位置描述")
    is_active = models.BooleanField(default=True, help_text="設備是否啟用")
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        verbose_name = "冰箱設備"
        verbose_name_plural = "冰箱設備"

    def __str__(self):
        return f"{self.name} ({self.device_id_esp})"

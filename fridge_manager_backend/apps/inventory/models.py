from django.conf import settings
from django.db import models

from apps.photos.models import Photo


class RecognizedItem(models.Model):
    photo = models.ForeignKey(
        Photo,
        on_delete=models.CASCADE,
        related_name='recognized_items',
        help_text="物品來源照片"
    )
    name = models.CharField(max_length=100, help_text="物品名稱")
    quantity = models.CharField(max_length=50, help_text="數量描述")
    estimated_expiry_info = models.TextField(help_text="預估保質期信息")
    placement_date = models.DateField(help_text="放置日期")
    owner = models.ForeignKey(
        settings.AUTH_USER_MODEL,
        on_delete=models.SET_NULL,
        null=True,
        related_name='owned_items',
        help_text="物品擁有者"
    )
    added_at = models.DateTimeField(auto_now_add=True, help_text="物品被記錄到系統的時間")
    notes = models.TextField(blank=True, help_text="附加說明")

    class Meta:
        verbose_name = "辨識物品"
        verbose_name_plural = "辨識物品"
        ordering = ['-added_at']

    def __str__(self):
        return f"{self.name} ({self.quantity}) - {self.owner.username if self.owner else '未知擁有者'}"

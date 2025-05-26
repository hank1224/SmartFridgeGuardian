from django.contrib import admin
from .models import FridgeDevice

@admin.register(FridgeDevice)
class FridgeDeviceAdmin(admin.ModelAdmin):
    list_display = ('name', 'device_id_esp', 'ip_address', 'port', 'is_active', 'created_at')
    list_filter = ('is_active', 'created_at')
    search_fields = ('name', 'device_id_esp', 'ip_address')
    ordering = ('-created_at',)

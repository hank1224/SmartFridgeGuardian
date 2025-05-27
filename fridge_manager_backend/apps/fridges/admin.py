from django.contrib import admin

from .models import FridgeDevice, FridgeOperationLog


@admin.register(FridgeDevice)
class FridgeDeviceAdmin(admin.ModelAdmin):
    list_display = ('name', 'device_id_esp', 'api_url', 'is_active', 'created_at')
    list_filter = ('is_active', 'created_at')
    search_fields = ('name', 'device_id_esp', 'api_url')
    ordering = ('-created_at',)

@admin.register(FridgeOperationLog)
class FridgeOperationLogAdmin(admin.ModelAdmin):
    list_display = ('user', 'fridge_device', 'operation_type', 'operation_start_time', 'photo_taken')
    list_filter = ('operation_type', 'operation_start_time', 'fridge_device')
    search_fields = ('user__username', 'fridge_device__name', 'notes')
    ordering = ('-operation_start_time',)
    readonly_fields = ('operation_start_time',)

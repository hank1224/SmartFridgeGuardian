from django.contrib import admin
from .models import Photo

@admin.register(Photo)
class PhotoAdmin(admin.ModelAdmin):
    list_display = ('id', 'fridge_device', 'uploaded_by', 'uploaded_at', 'recognition_status')
    list_filter = ('recognition_status', 'uploaded_at', 'fridge_device')
    search_fields = ('fridge_device__name', 'uploaded_by__username')
    ordering = ('-uploaded_at',)
    readonly_fields = ('uploaded_at', 'timestamp_esp')

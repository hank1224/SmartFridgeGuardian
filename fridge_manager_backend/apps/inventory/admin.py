from django.contrib import admin
from .models import RecognizedItem

@admin.register(RecognizedItem)
class RecognizedItemAdmin(admin.ModelAdmin):
    list_display = ('name', 'quantity', 'owner', 'placement_date', 'added_at')
    list_filter = ('placement_date', 'added_at', 'owner')
    search_fields = ('name', 'owner__username', 'notes')
    ordering = ('-added_at',)
    readonly_fields = ('added_at',)

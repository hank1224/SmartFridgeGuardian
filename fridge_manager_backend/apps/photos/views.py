from django.shortcuts import render, get_object_or_404
from django.contrib.auth.decorators import login_required
from .models import Photo

@login_required
def photo_list(request):
    """
    照片列表視圖
    """
    photos = Photo.objects.select_related('fridge_device', 'uploaded_by').all()
    return render(request, 'photos/photo_list.html', {
        'photos': photos
    })

@login_required
def photo_detail(request, photo_id):
    """
    照片詳情視圖
    """
    photo = get_object_or_404(
        Photo.objects.select_related('fridge_device', 'uploaded_by'),
        id=photo_id
    )
    items = photo.recognized_items.select_related('owner').all()
    return render(request, 'photos/photo_detail.html', {
        'photo': photo,
        'items': items
    })

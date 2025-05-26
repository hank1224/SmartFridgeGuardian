from django.urls import path
from . import views

app_name = 'fridges'

urlpatterns = [
    path('', views.fridge_list, name='list'),
    path('capture/<str:device_id>/', views.trigger_photo_capture, name='trigger_photo_capture'),
]
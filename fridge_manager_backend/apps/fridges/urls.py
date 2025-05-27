from django.urls import path

from . import views

app_name = 'fridges'

urlpatterns = [
    # 管理員視圖
    path('', views.fridge_list, name='list'),
    path('capture/<str:device_id>/', views.trigger_photo_capture, name='trigger_photo_capture'),

    # 用戶操作視圖
    path('user/', views.UserSelectFridgeView.as_view(), name='user_select_fridge'),
    path('user/<str:device_id>/operation/', views.UserSelectOperationView.as_view(), name='user_select_operation'),
    path('user/<str:device_id>/open/', views.UserOpenFridgeView.as_view(), name='user_open_fridge'),
]

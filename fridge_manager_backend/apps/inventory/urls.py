from django.urls import path

from . import views

app_name = 'inventory'

urlpatterns = [
    path('', views.item_list, name='list'),
    path('<int:item_id>/', views.item_detail, name='detail'),
    path('add/', views.item_add, name='add'),
    path('<int:item_id>/edit/', views.item_edit, name='edit'),
    path('<int:item_id>/delete/', views.item_delete, name='delete'),
    path('my-items/', views.UserItemListView.as_view(), name='user_items'),
]

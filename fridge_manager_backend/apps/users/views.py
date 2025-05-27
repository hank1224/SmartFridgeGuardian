from django.contrib import messages
from django.contrib.auth import login, logout
from django.shortcuts import redirect, render
from django.views.decorators.http import require_http_methods

from .forms import UserRegistrationForm

# Create your views here.

@require_http_methods(["GET", "POST"])
def logout_view(request):
    """
    自定義登出視圖
    """
    logout(request)  # 執行登出操作
    return render(request, 'users/logout.html')  # 顯示登出成功頁面

def register(request):
    """
    用戶註冊視圖
    """
    if request.method == 'POST':
        form = UserRegistrationForm(request.POST)
        if form.is_valid():
            user = form.save()
            login(request, user)
            messages.success(request, '註冊成功！歡迎使用智慧冰箱管理系統。')
            return redirect('core:home')
    else:
        form = UserRegistrationForm()

    return render(request, 'users/register.html', {
        'form': form,
        'title': '註冊'
    })

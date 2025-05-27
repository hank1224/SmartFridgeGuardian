from django.contrib.auth.decorators import login_required
from django.shortcuts import render

# Create your views here.

@login_required
def home(request):
    """
    首頁視圖
    """
    return render(request, 'core/home.html')

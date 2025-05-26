from django.shortcuts import render
from django.contrib.auth.decorators import login_required

# Create your views here.

@login_required
def home(request):
    """
    首頁視圖
    """
    return render(request, 'core/home.html')

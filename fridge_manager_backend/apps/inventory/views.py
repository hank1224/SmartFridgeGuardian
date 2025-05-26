from django.shortcuts import render, get_object_or_404, redirect
from django.contrib.auth.decorators import login_required
from django.contrib import messages
from .models import RecognizedItem
from .forms import RecognizedItemForm

# Create your views here.

@login_required
def item_list(request):
    """
    物品列表視圖
    """
    items = RecognizedItem.objects.select_related('photo', 'owner').all()
    return render(request, 'inventory/item_list.html', {
        'items': items
    })

@login_required
def item_detail(request, item_id):
    """
    物品詳情視圖
    """
    item = get_object_or_404(
        RecognizedItem.objects.select_related('photo', 'owner'),
        id=item_id
    )
    return render(request, 'inventory/item_detail.html', {
        'item': item
    })

@login_required
def item_add(request):
    """
    添加物品視圖
    """
    if request.method == 'POST':
        form = RecognizedItemForm(request.POST)
        if form.is_valid():
            item = form.save(commit=False)
            item.owner = request.user
            item.save()
            messages.success(request, '物品已成功添加')
            return redirect('inventory:detail', item_id=item.id)
    else:
        form = RecognizedItemForm()
    
    return render(request, 'inventory/item_form.html', {
        'form': form,
        'title': '添加物品'
    })

@login_required
def item_edit(request, item_id):
    """
    編輯物品視圖
    """
    item = get_object_or_404(RecognizedItem, id=item_id)
    
    if request.method == 'POST':
        form = RecognizedItemForm(request.POST, instance=item)
        if form.is_valid():
            form.save()
            messages.success(request, '物品已成功更新')
            return redirect('inventory:detail', item_id=item.id)
    else:
        form = RecognizedItemForm(instance=item)
    
    return render(request, 'inventory/item_form.html', {
        'form': form,
        'title': '編輯物品'
    })

@login_required
def item_delete(request, item_id):
    """
    刪除物品視圖
    """
    item = get_object_or_404(RecognizedItem, id=item_id)
    
    if request.method == 'POST':
        item.delete()
        messages.success(request, '物品已成功刪除')
        return redirect('inventory:list')
    
    return render(request, 'inventory/item_confirm_delete.html', {
        'item': item
    })

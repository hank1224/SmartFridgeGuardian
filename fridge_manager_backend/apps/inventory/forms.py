from django import forms
from .models import RecognizedItem

class RecognizedItemForm(forms.ModelForm):
    class Meta:
        model = RecognizedItem
        fields = ['name', 'quantity', 'estimated_expiry_info', 'notes']
        widgets = {
            'name': forms.TextInput(attrs={'class': 'form-control'}),
            'quantity': forms.TextInput(attrs={'class': 'form-control'}),
            'estimated_expiry_info': forms.Textarea(attrs={'class': 'form-control', 'rows': 3}),
            'notes': forms.Textarea(attrs={'class': 'form-control', 'rows': 3}),
        } 
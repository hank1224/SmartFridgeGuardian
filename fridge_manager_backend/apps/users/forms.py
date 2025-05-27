from django import forms
from django.contrib.auth.forms import UserCreationForm
from django.contrib.auth.models import User


class UserRegistrationForm(UserCreationForm):
    email = forms.EmailField(required=True, widget=forms.EmailInput(attrs={'class': 'form-control'}))

    class Meta:
        model = User
        fields = ('username', 'email', 'password1', 'password2')
        widgets = {
            'username': forms.TextInput(attrs={'class': 'form-control'}),
        }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # 自定義密碼欄位的樣式
        self.fields['password1'].widget.attrs.update({'class': 'form-control'})
        self.fields['password2'].widget.attrs.update({'class': 'form-control'})

        # 自定義錯誤訊息
        self.fields['username'].error_messages = {
            'required': '請輸入用戶名',
            'unique': '此用戶名已被使用',
        }
        self.fields['email'].error_messages = {
            'required': '請輸入電子郵件',
            'invalid': '請輸入有效的電子郵件地址',
        }
        self.fields['password1'].error_messages = {
            'required': '請輸入密碼',
        }
        self.fields['password2'].error_messages = {
            'required': '請確認密碼',
        }

from django.urls import path
from . import views

urlpatterns =[ path('alerts/api',views.get_alerts),
              path('readings/api',views.get_readings),
              path('dashboard',views.dashboard),

              ]


from django.shortcuts import render
from django.http import JsonResponse
from .models import Alert,SensorReading
# Create your views here.


def get_alerts(request):
    alerts = Alert.objects.all().order_by('-alert_times')[:4]

    data = []

    for alert in alerts:
        data.append({"message":alert.message,"alert_time":alert.time})

    JsonResponse({'alert':data})


def get_readings(request):
    readings = SensorReading.objects.all().order_by('-timestamp')[0]

    data = []

    for reading in readings:
        data.append({'timestamp':reading.timestamp,'temperature':reading.temperature,'pressure':reading.pressure,'humidity':reading.humidity,'soil_moisture':reading.soi        l_moisture,'light_intensity':reading.light_intensity})

        JsonResponse({'readings':data})



def dashboard(request):
     labels = [
    {"title": "Timestamp", "value_id": "timestamp-value"},
    {"title": "Pressure", "value_id": "pressure-value"},
    {"title": "Temperature", "value_id": "temperature-value"},
    {"title": "Humidity", "value_id": "humidity-value"},
    {"title": "Soil Moisture", "value_id": "soil-moisture-value"},
    {"title": "Light", "value_id": "light-value"},
    ]

    return render(request,"index.html",{"labels":labels})




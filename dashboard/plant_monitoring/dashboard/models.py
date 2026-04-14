from django.db import models

# Create your models here.


class Plant(models.Model):
    plant_name= models.CharField(max_length =100)



class Device(models.Model):
    plant = models.ForeignKey(Plant ,on_delete = models.CASCADE)
    mac_address = models.CharField(max_length = 17,unique = True)
    device_name = models.CharField(max_length = 100)

class SensorReading(models.Model):
    plant = models.ForeignKey(Plant,on_delete = models.CASCADE)
    device = models.ForeignKey(Device, on_delete = models.CASCADE)
    timestamp = models.DateTimeField()
    temperature = models.FloatField()
    pressure = models.FloatField()
    humidity  = models.FloatField()
    soil_moisture = models.FloatField()
    light_intensity = models.FloatField()


class Alert(models.Model):
    plant = models.ForeignKey(Plant,on_delete = models.CASCADE)
    alert_time = models.DateTimeField(null = True, blank = True)
    message = models.TextField()







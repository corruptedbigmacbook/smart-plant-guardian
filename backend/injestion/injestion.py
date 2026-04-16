import paho.mqtt.client as mqtt
from pathlib import Path
import json
from datetime import datetime 
import psycopg2 
#database adapter inistialisation
conn = psycopg2.connect("dbname=plantdb user=nanaowusu")

def save_to_postgres(payload):
    #helper funtion to save to postgres db:
    with conn.cursor()  as cur:
        try:
            data = json.loads(payload.strip()) 
        except json.JSONDecodeError as e:
            print("JSON ERROR:", e)
            return 
        cur.execute("SELECT id, plant_id from dashboard_device WHERE mac_address        = %s", (data["mac_address"],))

        result = cur.fetchone()
        if result is None:
            print("Device not registerd")
            return
        
        device_id = result[0]
        plant_id = result[1]

        #database operation to insert into alerts table 
        try:
            if data["soil_moisture"] > 80:
                cur.execute("""INSERT INTO  dashboard_alert (message, plant_id) VALUES (%s, %s)""", ("overwatered", plant_id))
            elif data["soil_moisture"]< 40:
                cur.execute("""INSERT INTO  dashboard_alert (message,plant_id)  VALUES (%s, %s)""",( "underwatered", plant_id))
    
        #database operation to insert into sensor reading table
            cur.execute(""" 
            INSERT INTO  dashboard_sensor_reading (timestamp, temperature, pressure, humidity, soil_moisture, light_intensity, device_id) VALUES (%(timestamp)s, %(temperature)s, %(pressure)s, %(humidity)s, %(soil_moisture)s, %(light_intensity)s,%(device_id)s);""",{"timestamp": data['timestamp'], "temperature": data['temperature'], "pressure": data["pressure"],"humidity": data["humidity"], "soil_moisture":data['soil_moisture'], "light_intensity": data['light_intensity'], "device_id": device_id})
            conn.commit()
            print("Saved to PostgreSQL")

        except Exception as e:
            conn.rollback()
            print("database error:" , e)
#helper function to save to local jsonl directory grouped by date
def save_to_jsonl(payload):
    try:
        data = json.loads(payload.strip()) 
    except json.JSONDecodeError as e:
        print("JSON ERROR:", e)
        return 
    try:
        mqtt_time = data["timestamp"]
    except KeyError:
        print("Missing timestamp field")
        return 

    try:
        dt = datetime.fromisoformat(mqtt_time)
    except ValueError:
        print("invalid timestamp format ")
        return 

    date_str = dt.date().isoformat()
    path = Path(f"data/{date_str}.jsonl")
    path.parent.mkdir(parents=True,exists_ok=True)

    with open(path,'a') as f:
        f.write(json.dumps(data)+ '\n' )       


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    save_to_jsonl(payload)
    save_to_postgres(payload)
def on_connect(client, userdata, flags, reason_code, properties):
    print("Connected")
    client.subscribe("esp32/data")

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.connect("test.mosquitto.org",1883)

mqttc.loop_forever()


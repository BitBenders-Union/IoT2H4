import paho.mqtt.client as mqtt
import psycopg2
from datetime import datetime

# PostgreSQL connection settings
DB_SETTINGS = {
    "dbname": "CamCounter",
    "user": "postgres",
    "password": "",
    "host": "192.168.0.153",
    "port": 5432
}

# MQTT connection settings
MQTT_BROKER = "localhost"  # Mosquitto broker address (e.g., IP or hostname)
MQTT_PORT = 1883           # Default MQTT port
MQTT_TOPIC = "your/topic"  # Topic to subscribe to

# Connect to PostgreSQL
def connect_to_db():
    try:
        conn = psycopg2.connect(**DB_SETTINGS)
        return conn
    except Exception as e:
        print(f"Error connecting to database: {e}")
        return None

# Insert data into PostgreSQL
def insert_into_db(payload):
    try:
        conn = connect_to_db()
        if conn is None:
            return

        cursor = conn.cursor()
        query = """
            INSERT INTO your_table (timestamp, topic, message)
            VALUES (%s, %s, %s)
        """
        cursor.execute(query, (datetime.now(), MQTT_TOPIC, payload))
        conn.commit()
        cursor.close()
        conn.close()
        print(f"Inserted data into database: {payload}")
    except Exception as e:
        print(f"Error inserting into database: {e}")

# MQTT callback when connected
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker!")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")

# MQTT callback when a message is received
def on_message(client, userdata, msg):
    print(f"Received message: {msg.payload.decode()} from topic: {msg.topic}")
    insert_into_db(msg.payload.decode())

# Main function
def main():
    # Initialize MQTT client
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        # Connect to MQTT broker
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()  # Start the MQTT loop
    except Exception as e:
        print(f"Error connecting to MQTT broker: {e}")

if __name__ == "__main__":
    main()

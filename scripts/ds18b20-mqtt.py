import paho.mqtt.client as mqtt
import psycopg2
import json

# PostgreSQL connection parameters
DB_HOST = "localhost"
DB_NAME = "DS18B20"
DB_USER = "mqtt-broker"
DB_PASSWORD = "dietpi"
TABLE_NAME = "temperature_data"  # Ensure this table exists

# MQTT broker parameters
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC = "home/ds18b20"

# Connect to PostgreSQL
def connect_db():
    try:
        connection = psycopg2.connect(
            host=DB_HOST,
            database=DB_NAME,
            user=DB_USER,
            password=DB_PASSWORD
        )
        print("Connected to PostgreSQL database")
        return connection
    except Exception as e:
        print(f"Error connecting to PostgreSQL: {e}")
        exit(1)

# Insert data into PostgreSQL
def insert_data(connection, temperature, timestamp):
    try:
        cursor = connection.cursor()
        query = f"""
        INSERT INTO {TABLE_NAME} (temperature, time)
        VALUES (%s, %s);
        """
        cursor.execute(query, (temperature, timestamp))
        connection.commit()
        print("Data inserted successfully")
    except Exception as e:
        print(f"Error inserting data into PostgreSQL: {e}")
        connection.rollback()

# MQTT on_connect callback
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")

# MQTT on_message callback
def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        print(f"Received message: {payload}")
        temperature = payload.get("temperature")
        timestamp = payload.get("time")

        if temperature and timestamp:
            insert_data(userdata["db_connection"], temperature, timestamp)
        else:
            print("Invalid data received")
    except Exception as e:
        print(f"Error processing message: {e}")

def main():
    # Connect to PostgreSQL
    db_connection = connect_db()

    # Set up MQTT client
    mqtt_client = mqtt.Client(userdata={"db_connection": db_connection})
    mqtt_client.username_pw_set("mosquitto", "dietpi")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    # Connect to the MQTT broker
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"Error connecting to MQTT broker: {e}")
    finally:
        db_connection.close()
        print("PostgreSQL connection closed")

if __name__ == "__main__":
    main()

import eventlet
eventlet.monkey_patch()

from flask import Flask, jsonify, request
from flask_socketio import SocketIO, emit
from flask_cors import CORS
import psycopg2
import json

# Initialize Flask app and socketio with eventlet support
app = Flask(__name__)
socketio = SocketIO(app, async_mode='eventlet', cors_allowed_origins="*")
CORS(app, resources={r"/*": {"origins": "*"}})

# Database connection details
DHT_DB_HOST = "192.168.0.153"
DHT_DB_NAME = "DHT-11"
DHT_DB_USER = "mqtt-broker"
DHT_DB_PASSWORD = "dietpi"

# Function to get the latest DHT data
def get_latest_dht_data():
    try:
        conn = psycopg2.connect(host=DHT_DB_HOST, dbname=DHT_DB_NAME, user=DHT_DB_USER, password=DHT_DB_PASSWORD)
        cursor = conn.cursor()
        cursor.execute("SELECT id, temperature, humidity FROM sensor_data ORDER BY time DESC LIMIT 1;")
        result = cursor.fetchone()
        cursor.close()
        conn.close()
        if result:
            return {
                "id": result[0],
                "temperature": result[1],
                "humidity": result[2]
            }
    except Exception as e:
        print(f"Error fetching data from database: {e}")
    return None 

# WebSocket event for handling client connection
@socketio.on('connect')
def handle_connect():
    print("Client connected")
    
    # Send the latest data when a client connects
    latest_data = get_latest_dht_data()
    if latest_data:
        emit('update_data', latest_data)
    
    # Start the background task to listen for new data from the database
    socketio.start_background_task(listen_for_new_data)

# WebSocket event to listen for new data
def listen_for_new_data():
    print("Listening for new data...")
    
    try:
        conn = psycopg2.connect(host=DHT_DB_HOST, dbname=DHT_DB_NAME, user=DHT_DB_USER, password=DHT_DB_PASSWORD)
        cursor = conn.cursor()
        cursor.execute("LISTEN new_data;")  # Listening for a PostgreSQL event
        while True:
            conn.poll()
            while conn.notifies:
                notify = conn.notifies.pop(0)
                print(f"Received notification: {notify.payload}")
                # Parse the payload and emit the data
                new_data = json.loads(notify.payload)  # Assuming the payload is JSON
                new_data.pop("time", None)  # Remove "time" field if it exists
                socketio.emit('update_data', new_data)  # Emit the new data to all connected clients
            socketio.sleep(1)  # Sleep for a bit to avoid tight loop
    except Exception as e:
        print(f"Error while listening for new data: {e}")


# Test route to simulate new data notification
@app.route("/test_notification", methods=["POST"])
def test_notification():
    # Simulate new data payload
    test_data = {
        "id": 999,
        "temperature": 23.5,
        "humidity": 55.2
    }
    # Emit the data to all connected clients
    socketio.emit('update_data', test_data)
    return jsonify({"message": "Test notification sent", "data": test_data})

# Chatbot route to interact with the user
@app.route("/chat", methods=["POST"])
def chat():
    user_input = request.json.get('message')
    if user_input:
        # Simple bot response logic
        if "hello" in user_input.lower():
            bot_response = "Hello! How can I assist you today?"
        else:
            bot_response = "I'm not sure how to respond to that. Can you ask something else?"
        return jsonify({"response": bot_response})
    return jsonify({"response": "No input provided."})

# Main entry point for the application
if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000)  # Eventlet is automatically used by setting async_mode='eventlet'

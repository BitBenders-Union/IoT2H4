from flask import Flask, jsonify
from flask_cors import CORS
import psycopg2
from datetime import datetime, timedelta

app = Flask(__name__)

# Enable CORS for all routes
CORS(app)

# Database connection setup for DHT-11
def get_db_connection_dht():
    conn = psycopg2.connect(
        host="localhost",  # Database host
        database="DHT-11",  # The DHT-11 database you're connecting to
        user="mqtt-broker",  # Database user
        password="dietpi"  # Database password
    )
    return conn

# Database connection setup for SuperSonic
def get_db_connection_super_sonic():
    conn = psycopg2.connect(
        host="localhost",  # Database host
        database="SuperSonic",  # The SuperSonic database you're connecting to
        user="mqtt-broker",  # Database user
        password="dietpi"  # Database password
    )
    return conn

# Query for the latest temperature and humidity data from DHT-11 table
@app.route('/get_latest_data', methods=['GET'])
def get_latest_data():
    conn = get_db_connection_dht()  # Use DHT-11 database connection
    cur = conn.cursor()
    
    # Fetch latest temperature and humidity from DHT-11 table
    cur.execute('SELECT temperature, humidity, time FROM sensor_data ORDER BY time DESC LIMIT 1;')
    latest_data = cur.fetchone()
    cur.close()
    
    if latest_data:
        temperature, humidity, time = latest_data
        print(f"Fetched temperature={temperature}°C, humidity={humidity}%, time={time}")
        return jsonify({
            'temperature': temperature,
            'humidity': humidity,
            'time': time
        })
    
    return jsonify({"error": "No data available"}), 500

# Query to fetch human count for today and this week from super_sonic table
@app.route('/get_human_count', methods=['GET'])
def get_human_count():
    conn = get_db_connection_super_sonic()  # Use SuperSonic database connection
    cur = conn.cursor()

    # Get current date and calculate start of the week (Monday)
    today = datetime.today()
    start_of_week = today - timedelta(days=today.weekday())  # Start of the current week (Monday)

    print(f"WeeklyTime: ", {start_of_week})
    print(f"Today: ", {today})

    # Convert times to string format for easier debugging (you can remove in production)
    print(f"Today: {today}, Start of Week: {start_of_week}")

    # Get human count for today (from 00:00 to current time)
    cur.execute("""
        SELECT SUM(person) FROM super_sonic
        WHERE time::date = %s;
    """, (today.date(),))
    today_count = cur.fetchone()[0] or 0  # Default to 0 if no data found
    print(f"Today count: {today_count}")  # Debugging line to see fetched data

    # Get human count for the current week (from Monday to today)
    cur.execute("""
        SELECT SUM(person) FROM super_sonic
        WHERE time >= %s AND time < %s;
    """, (start_of_week, today))
    weekly_count = cur.fetchone()[0] or 0  # Default to 0 if no data found
    print(f"Weekly count: {weekly_count}")  # Debugging line to see fetched data

    cur.close()

    return jsonify({
        'today_count': today_count,
        'weekly_count': weekly_count
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=True)

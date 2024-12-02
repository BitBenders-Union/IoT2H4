# IoT2H4

## Beskrivelse:
Målet med projektet er at demonstrere, hvordan man kan opsætte et system, hvor data fra sensorer (som temperaturmålinger og personer tæller) sendes via MQTT-protokollen og gemmes i en PostgreSQL-database for videre analyse eller præsentation.

## Gruppe
- Jonas
- Lucas
- Sanjit
- Dennis

## Komponenter
- 2 stk: ESP32 – Dev board
- 1 stk: DHT11 – Temperaturmåler
- 1 stk: UltraSonic sensor
- 1 stk: Raspberry Pi 4B

## Software på Pi
- **DietPi OS**
  - Mosquitto (MQTT broker)
  - PostgreSQL (Database)
  - Nginx (Webserver)
  - Flask (Micro web framework)
  - Python 3
    
## MQTT Broker Scripts
Vi har 2 scripts til opsætning og håndtering af vores MQTT broker:
- **dht-11-mqtt.py**
Dette script lytter på MQTT-topikken `home/dht`, modtager JSON-beskeder med temperatur, luftfugtighed og tidsstempel, og indsætter dataene i en PostgreSQL-database.

  - **Funktion**: Modtager DHT-11 sensor data (temperatur, luftfugtighed) via MQTT og gemmer det i PostgreSQL.
  - **Fejlbehandling**: Håndterer forbindelses- og indsættelsesfejl.
   Forbinder mqtt broker til vores database

- **ultraSonic-mqtt.py**
Dette script lytter på MQTT-topikken `esp32/people_counter`, modtager JSON-beskeder og indsætter data (tidspunkt og antal personer) i en PostgreSQL-database. Det bruger `paho.mqtt` til MQTT-forbindelse og `psycopg2` til databaseforbindelse.

  - **Funktion**: Modtager data via MQTT og gemmer det i PostgreSQL.
  - **Fejlbehandling**: Håndterer forbindelses- og indsættelsesfejl.

## Kildekode DHT11 og ultrasonic sensors
- [DHT11 - Kode](https://github.com/BitBenders-Union/IoT2H4/blob/main/DHT11%20-%20Standalone/src/main.cpp)
- [UltraSonic - kode](https://github.com/BitBenders-Union/IoT2H4/blob/main/SuperSonice/src/main.cpp)


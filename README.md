# IoT2H4

## Beskrivelse:
Målet med projektet er at demonstrere, hvordan man kan opsætte et system, hvor data fra sensorer (som temperaturmålinger og personer tæller) sendes via MQTT-protokollen og gemmes i en PostgreSQL-database for videre analyse eller præsentation.

## Gruppe
- Jonas
- Lucas
- Sanjit
- Dennis

## Komponenter
- 4 stk: ESP32 – Dev board
- 1 stk: DHT11 – Temperaturmåler
- 1 stk: DS18B20 - Temperaturmåler
- 1 stk: UltraSonic sensor
- 1 stk: RFID - RC522 sensor
- 1 stk: Raspberry Pi 4B

## Software på Pi
- **DietPi OS**
  - Mosquitto (MQTT broker)
  - PostgreSQL (Database)
  - Nginx (Webserver)
  - Flask (Micro web framework)
  - Python 3

# **Quickguide til IoT2H4**

## **1. Hardwareopsætning**
1. Tilslut DHT11-sensoren til det første ESP32-dev board.
2. Tilslut DS18B20 til det andet ESP32-dev board.
3. Tilslut ultralydssensoren til det trejde ESP32-dev board.
4. Tilslut RFID-RC522 sensoren til det fjere ESP32-dev board.
5. Sørg for, at Raspberry Pi 4B er korrekt sat op og tilsluttet netværket.

## **2. Softwareopsætning på Raspberry Pi**
1. Installer **DietPi OS** på Raspberry Pi.
2. Installer følgende software via DietPi's Software Optimized:
   - **Mosquitto** (MQTT broker)
   - **PostgreSQL** (Database)
   - **Nginx** (Webserver)
   - **Python 3** og **Flask** (Micro web framework)
3. Opret databaser og tabeller i PostgreSQL:
   - `DHT-11` database med en `sensor_data` tabel (felter: `temperature`, `humidity`, `time`).
   - `SuperSonic` database med en `super_sonic` tabel (felter: `time`, `person`).

## **3. Deployment af Scripts**
1. Kopier følgende Python-scripts til Raspberry Pi:
   - `dht-11-mqtt.py` til håndtering af DHT11-sensorens data.
   - `ultraSonic-mqtt.py` til håndtering af ultralydssensorens data.
     
## **4. Deployment af Scripts som Services**
1. Opret systemd-services for de to scripts, så de starter automatisk ved opstart:
   - Lav en servicefil til hvert script (`dht-11-mqtt.service` og `ultraSonic-mqtt.service`).
   - Angiv scriptets sti og sørg for, at det genstarter automatisk ved fejl.

2. Aktiver og start tjenesterne:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable dht-11-mqtt.service
   sudo systemctl enable ultraSonic-mqtt.service
   sudo systemctl start dht-11-mqtt.service
   sudo systemctl start ultraSonic-mqtt.service

## **5. Opsætning af Webserver**
   - Kopier filerne fra vores repository (mappe: `ngnix`) til Nginx's serveringsmappe på Raspberry Pi.
   - Genstart webserveren
   


# MQTT Broker Scripts
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
- [Ds18B20 - kode](https://github.com/BitBenders-Union/IoT2H4/blob/main/DS18B20_Sensor/src/main.cpp)
- [UltraSonic - kode](https://github.com/BitBenders-Union/IoT2H4/blob/main/SuperSonice/src/main.cpp)
- [CardReader - kode](https://github.com/BitBenders-Union/IoT2H4/blob/main/CardReader/src/main.cpp)


## kredsløbsdiagram 
- [DHT11 - diagram](https://github.com/BitBenders-Union/IoT2H4/blob/main/Doc/DHT11Sensor_bb.png)
- [DS18B20 - diagram](https://github.com/BitBenders-Union/IoT2H4/blob/main/Doc/Ds18b20.png)
- [UltraSonic - diagram](https://github.com/BitBenders-Union/IoT2H4/blob/main/Doc/ultraSonicSensor_bb.png)
- [CardReader - diagram](https://github.com/BitBenders-Union/IoT2H4/blob/main/Doc/Cardreader_bb.png)






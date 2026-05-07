# ESP32-RFID-Attendance-System-with-Web-Dashboard-and-Cloud-Integration

An IoT-based RFID attendance system using ESP32, EM-18 RFID reader, OLED display, web dashboard, Firebase cloud logging, and realtime attendance monitoring.

---

## Features

* RFID Attendance System
* OLED Status Display
* Realtime Web Dashboard
* AP Mode + WiFi Mode
* Firebase Cloud Logging
* User Registration & Removal
* EEPROM/Preferences Storage
* Mobile Friendly UI
* LED + Buzzer Indication
* Realtime Time Updates

---

## Hardware Used

* ESP32 Dev Board
* EM-18 RFID Reader
* SSD1306 OLED Display
* LED
* Buzzer
* RFID Tags/Cards

---

## Connections

### EM-18 RFID

| EM-18 | ESP32  |
| ----- | ------ |
| TX    | GPIO16 |
| GND   | GND    |
| VCC   | 5V     |

### OLED SSD1306

| OLED | ESP32  |
| ---- | ------ |
| SDA  | GPIO21 |
| SCL  | GPIO22 |
| VCC  | 3.3V   |
| GND  | GND    |

### LED

| LED | ESP32            |
| --- | ---------------- |
| +   | GPIO2            |
| -   | GND via resistor |

### Buzzer

| Buzzer | ESP32 |
| ------ | ----- |
| +      | GPIO4 |
| -      | GND   |

---

## AP Mode

If WiFi is unavailable, ESP32 starts hotspot mode.

SSID:

```txt
ESP32_RFID
```

Password:

```txt
12345678
```

Open:

```txt
http://192.168.4.1
```

---

## Firebase Features

* Cloud attendance logs
* Name
* UID
* Time
* Status

---

## Web Dashboard Features

* Register RFID cards
* Remove users
* Realtime attendance updates
* Mobile responsive UI

---

## Libraries Used

* WiFi.h
* WebServer.h
* Preferences.h
* Adafruit_GFX.h
* Adafruit_SSD1306.h
* Firebase_ESP_Client.h

---

## Author

Roshan Chavan

Embedded Systems & IoT Project


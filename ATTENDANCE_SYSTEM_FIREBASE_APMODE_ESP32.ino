/*
========================================================
        ESP32 RFID Attendance System with Web Dashboard
========================================================

FEATURES:
---------
1. RFID Attendance using EM-18 Reader
2. OLED Display Status
3. Web Dashboard (Mobile Friendly)
4. AP Mode + WiFi Mode
5. User Registration & Removal
6. EEPROM/Preferences Storage
7. Firebase Cloud Logging
8. Real Time Clock (NTP Time)
9. LED + Buzzer Indication
10. Realtime Dashboard Updates

--------------------------------------------------------
HARDWARE USED:
--------------------------------------------------------

1. ESP32 Dev Board
2. EM-18 RFID Reader
3. SSD1306 OLED Display (I2C)
4. LED
5. Buzzer
6. RFID Tags/Cards

--------------------------------------------------------
PIN CONNECTIONS
--------------------------------------------------------

============== EM-18 RFID ==============

EM-18 TX   ---> ESP32 GPIO16 (RX2)
EM-18 GND  ---> ESP32 GND
EM-18 VCC  ---> 5V

============== OLED SSD1306 ==============

OLED VCC   ---> ESP32 3.3V
OLED GND   ---> ESP32 GND
OLED SCL   ---> ESP32 GPIO22
OLED SDA   ---> ESP32 GPIO21

============== LED ==============

LED +ve ---> GPIO2
LED -ve ---> GND through 220 ohm resistor

============== BUZZER ==============

Buzzer +ve ---> GPIO4
Buzzer -ve ---> GND

--------------------------------------------------------
WORKING:
--------------------------------------------------------

1. ESP32 starts in STA + AP mode
2. Tries connecting to WiFi
3. If WiFi connected:
      - Firebase enabled
      - Cloud logging enabled
      - OLED shows CLOUD mode
4. If WiFi fails:
      - ESP32 starts hotspot
      - OLED shows AP MODE
5. RFID card scanned
6. UID checked in registered user list
7. If user exists:
      - OLED shows GRANTED
      - LED ON
      - Single buzzer beep
8. If unknown:
      - OLED shows DENIED
      - Double buzzer beep
9. Scan data uploaded to Firebase
10. Web dashboard updates in realtime

--------------------------------------------------------
DEFAULT AP MODE DETAILS
--------------------------------------------------------

SSID     : ESP32_RFID
Password : 12345678

Open Browser:
http://192.168.4.1

--------------------------------------------------------
FIREBASE FEATURES
--------------------------------------------------------

1. Cloud attendance logs
2. Name
3. UID
4. Time
5. Status

--------------------------------------------------------
WEB DASHBOARD FEATURES
--------------------------------------------------------

1. Register new RFID card
2. Remove users
3. Realtime attendance
4. Mobile friendly UI
5. Live time updates

--------------------------------------------------------
IMPORTANT LIBRARIES
--------------------------------------------------------

1. WiFi.h
2. WebServer.h
3. Preferences.h
4. Adafruit_GFX.h
5. Adafruit_SSD1306.h
6. Firebase_ESP_Client.h

--------------------------------------------------------
AUTHOR:
--------------------------------------------------------

Project by Roshan Chavan
Embedded Systems & IoT Project

========================================================
*/
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Firebase_ESP_Client.h>
#include "time.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LED 2
#define BUZZER 4
///-----------firebase------------
#define API_KEY "ADD_FIREBASE_API_KEY"
#define DATABASE_URL "ADD_FIREBASE_DATABASE_URL_KEY"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
//-------------------------------------
HardwareSerial mySerial(2);

WebServer server(80);
Preferences prefs;

#define MAX_USERS 10

struct User {
  String uid;
  String name;
};

User users[MAX_USERS];
int userCount = 0;

String lastUID = "";
String lastName = "";
String lastStatus = "";
String lastTime = "";

// -------- TIME --------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// -------- USER MGMT --------
int findUser(String uid) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].uid == uid) return i;
  }
  return -1;
}

void saveUsers() {
  prefs.putUInt("count", userCount);
  for (int i = 0; i < userCount; i++) {
    prefs.putString(("uid" + String(i)).c_str(), users[i].uid);
    prefs.putString(("name" + String(i)).c_str(), users[i].name);
  }
}

void loadUsers() {
  userCount = prefs.getUInt("count", 0);
  for (int i = 0; i < userCount; i++) {
    users[i].uid = prefs.getString(("uid" + String(i)).c_str(), "");
    users[i].name = prefs.getString(("name" + String(i)).c_str(), "");
  }
}

// -------- WEB UI --------
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body{font-family:Arial;text-align:center;background:#f2f2f2;}
    .box{background:white;margin:10px;padding:15px;border-radius:10px;}
    button{padding:10px;border:none;border-radius:5px;background:green;color:white;}
    .del{background:red;}
  </style>
  <script>
    function load(){
      fetch('/data')
      .then(r=>r.json())
      .then(d=>{
        document.getElementById("name").innerText=d.name;
        document.getElementById("status").innerText=d.status;
        document.getElementById("time").innerText=d.time;
      });
    }
    setInterval(load,1000);
  </script>
  </head>

  <body onload="load()">

  <h2>RFID System</h2>

  <div class="box">
    <p>Name/UID: <span id="name"></span></p>
    <p>Status: <span id="status"></span></p>
    <p>Time: <span id="time"></span></p>
  </div>

  <div class="box">
    <form action="/register">
      <input name="name" placeholder="Enter Name">
      <button>Register</button>
    </form>
  </div>

  <div class="box">
  )rawliteral";

  for (int i = 0; i < userCount; i++) {
    html += users[i].name + " (" + users[i].uid + ") ";
    html += "<a href='/remove?uid=" + users[i].uid + "'><button class='del'>Remove</button></a><br>";
  }

  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

// -------- API --------
void handleData() {
  String json = "{";
  json += "\"name\":\"" + lastName + "\",";
  json += "\"status\":\"" + lastStatus + "\",";
  json += "\"time\":\"" + lastTime + "\"}";
  server.send(200, "application/json", json);
}

// -------- REGISTER --------
void handleRegister() {
  if (server.hasArg("name") && lastUID != "" && findUser(lastUID) == -1) {
    if (userCount < MAX_USERS) {
      users[userCount].uid = lastUID;
      users[userCount].name = server.arg("name");
      userCount++;
      saveUsers();
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// -------- REMOVE --------
void handleRemove() {
  if (server.hasArg("uid")) {
    String uid = server.arg("uid");
    for (int i = 0; i < userCount; i++) {
      if (users[i].uid == uid) {
        for (int j = i; j < userCount - 1; j++) {
          users[j] = users[j + 1];
        }
        userCount--;
        saveUsers();
        break;
      }
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}
void uploadLog(String name, String uid, String status, String time) {

  FirebaseJson json;

  json.set("name", name);
  json.set("uid", uid);
  json.set("status", status);
  json.set("time", time);

  String path = "/logs";

  if (Firebase.RTDB.pushJSON(&fbdo, path, &json)) {

    Serial.println("Cloud Upload Success");

  } 
  else {

    Serial.println("Upload Failed");
    Serial.println(fbdo.errorReason());
  }
}
void updateTime() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    lastTime = "Time Error";
    return;
  }

  char timeString[20];

  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);

  lastTime = String(timeString);
}
// ---------- WIFI ----------
const char* wifi_ssid = "Roshan";
const char* wifi_password = "12345678";

// ---------- AP ----------
const char* ap_ssid = "ESP32_RFID";
const char* ap_password = "12345678";
bool signupOK = false;
// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, 16, -1);

  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);

  prefs.begin("rfid", false);
  loadUsers();
// ---------- WiFi ----------
WiFi.mode(WIFI_MODE_APSTA);

// OLED Message
display.clearDisplay();
display.setTextSize(2);
display.setCursor(0, 10);
display.println("Connecting");
display.setCursor(0, 35);
display.println("WiFi...");
display.display();

// Connect Internet WiFi
WiFi.begin(wifi_ssid, wifi_password);

Serial.print("Connecting to WiFi");

// 3 minute timeout
unsigned long startAttempt = millis();

while (WiFi.status() != WL_CONNECTED &&
       millis() - startAttempt < 15000) {

  delay(500);

  Serial.print(".");
}

// ---------- WIFI CONNECTED ----------
if (WiFi.status() == WL_CONNECTED) {

  Serial.println("\nWiFi Connected");

  Serial.print("STA IP: ");
  Serial.println(WiFi.localIP());

  // OLED
  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 10);
  display.println("WiFi Connected");

  display.setCursor(0, 30);
  display.println(WiFi.localIP());

  display.display();

  delay(3000);

} else {

  // ---------- WIFI FAILED ----------
  // STOP WIFI RETRY
  WiFi.disconnect(true);

  Serial.println("\nWiFi Failed");

  display.clearDisplay();

  display.setTextSize(2);

  display.setCursor(0, 5);
  display.println("WiFi");

  display.setCursor(0, 30);
  display.println("Failed");

  display.display();

  delay(800);
}

// ---------- AP MODE ----------


bool apStarted = WiFi.softAP(ap_ssid, ap_password);

delay(1000);

if (apStarted) {

  Serial.println("AP Started");

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

} else {

  Serial.println("AP Failed");
}


// ---------- NTP ----------
configTime(gmtOffset_sec,
           daylightOffset_sec,
           ntpServer);

config.api_key = API_KEY;
config.database_url = DATABASE_URL;

// ================= FIREBASE =================

if (WiFi.status() == WL_CONNECTED) {

  config.api_key = API_KEY;

  config.database_url = DATABASE_URL;

  // Anonymous SignUp
  if (Firebase.signUp(&config,
                      &auth,
                      "",
                      "")) {

    Serial.println("Firebase SignUp OK");

    signupOK = true;

  } else {

    Serial.println(
      config.signer
      .signupError
      .message
      .c_str()
    );
  }

  // START FIREBASE
  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);
}
// ================= MODE DISPLAY =================

String ipText;

display.clearDisplay();

display.setTextColor(WHITE);

// ---------- MODE ----------
display.setTextSize(2);

if (WiFi.status() == WL_CONNECTED) {

  display.setCursor(15, 0);
  display.println("CLOUD");

  ipText = WiFi.localIP().toString();

} else {

  display.setCursor(5, 0);
  display.println("AP MODE");

  ipText = WiFi.softAPIP().toString();
}

// ---------- IP DISPLAY ----------
display.setTextSize(2);

int16_t x1, y1;
uint16_t w, h;

display.getTextBounds(
  ipText,
  0,
  0,
  &x1,
  &y1,
  &w,
  &h
);

// ---------- CENTER IF FIT ----------
if (w <= SCREEN_WIDTH) {

  int x = (SCREEN_WIDTH - w) / 2;

  display.setCursor(x, 30);

  display.println(ipText);

  display.display();

  delay(2000);

} else {

  // ---------- SCROLL ----------
  for (int x = SCREEN_WIDTH;
       x > -w;
       x -= 2) {

    // Clear only IP area
    display.fillRect(
      0,
      30,
      SCREEN_WIDTH,
      25,
      BLACK
    );

    display.setCursor(x, 30);

    display.println(ipText);

    display.display();

    delay(40);
  }
}

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/register", handleRegister);
  server.on("/remove", handleRemove);

  server.begin();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println("Scan Card");
  display.display();
}

// -------- LOOP --------
void loop() {

  server.handleClient();

  if (mySerial.available()) {

    String card = "";

    while (mySerial.available()) {
      char c = mySerial.read();
      if (c != '\n' && c != '\r') card += c;
      delay(5);
    }

    lastUID = card;

    int index = findUser(card);

    if (index != -1) {
      lastStatus = "GRANTED";
      lastName = users[index].name;

      digitalWrite(LED, HIGH);
      digitalWrite(BUZZER, HIGH);
      delay(300);
      digitalWrite(BUZZER, LOW);

    } else {
      lastStatus = "DENIED";
      lastName = card;

      for (int i = 0; i < 2; i++) {
        digitalWrite(BUZZER, HIGH);
        delay(200);
        digitalWrite(BUZZER, LOW);
        delay(200);
      }
    }

// TIME
updateTime();

// ================= OLED FIRST =================

display.clearDisplay();

display.setTextSize(2);

display.setCursor(10, 5);

display.println(lastStatus);

display.setTextSize(1);

display.setCursor(10, 40);

display.println(lastName);

display.display();

// ================= CLOUD AFTER DISPLAY =================

if (WiFi.status() == WL_CONNECTED) {

  uploadLog(
    lastName,
    lastUID,
    lastStatus,
    lastTime
  );
}

    delay(2000);

    digitalWrite(LED, LOW);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.println("Scan Card");
    display.display();
  }
}

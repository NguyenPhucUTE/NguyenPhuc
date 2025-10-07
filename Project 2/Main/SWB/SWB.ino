#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <FirebaseESP32.h>
#include <DHT.h>

#define WIFI_SSID "Boi"
#define WIFI_PASSWORD "14051999"
#define FIREBASE_HOST "tt-iot-5a3bd-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyA1Nq1PfqUB9fGQpImxEN9vao4jVO13oVM"

#define DHTPIN 23
#define DHTTYPE DHT11
#define RELAY_PIN 16
#define POWER_PIN 32
#define SIGNAL_PIN 35
#define THRESHOLD 1000

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200, 60000);
WebServer server(80);
Servo myServo;
int servoPin = 13;
Preferences preferences;

int webOnHour;
int webOnMinute;
int webOffHour;
int webOffMinute;

DHT dht(DHTPIN, DHTTYPE);
FirebaseData firebaseData;
FirebaseData fbdo;

void setup() {
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  myServo.attach(servoPin);
  myServo.write(0);

  timeClient.begin();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/26/on", handleFeed);
  server.begin();

  loadSettings();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
}
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Servo Timer</title></head><body>";
  html += "<h1>Servo Timer</h1>";
  html += "<form action='/set' method='post'>";
  html += "Morning (HH:MM): <input type='text' name='onHour'>:<input type='text' name='onMinute'><br>";
  html += "Evening(HH:MM): <input type='text' name='offHour'>:<input type='text' name='offMinute'><br>";
  html += "<input type='submit' value='Set Timer'>";
  html += "</form>";
  html += "<a href='/26/on'>Feed Now</a><br>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSet() {
  String onHourStr = server.arg("onHour");
  String onMinuteStr = server.arg("onMinute");
  String offHourStr = server.arg("offHour");
  String offMinuteStr = server.arg("offMinute");

  webOnHour = onHourStr.toInt();
  webOnMinute = onMinuteStr.toInt();
  webOffHour = offHourStr.toInt();
  webOffMinute = offMinuteStr.toInt();

  saveSettings();

  server.send(200, "text/plain", "Timer set successfully");
}

void handleFeed() {
  activateServo();
  server.send(200, "text/plain", "Feeding now");
}

void activateServo() {
  myServo.write(45);
  delay(2000);
  myServo.write(0);
}

void saveSettings() {
  preferences.begin("servo_settings", false);
  preferences.putUInt("onHour", webOnHour);
  preferences.putUInt("onMinute", webOnMinute);
  preferences.putUInt("offHour", webOffHour);
  preferences.putUInt("offMinute", webOffMinute);
  preferences.end();
}

void loadSettings() {
  preferences.begin("servo_settings", true);
  webOnHour = preferences.getUInt("onHour", 0);
  webOnMinute = preferences.getUInt("onMinute", 0);
  webOffHour = preferences.getUInt("offHour", 0);
  webOffMinute = preferences.getUInt("offMinute", 0);
  preferences.end();
}

void loop() {
  server.handleClient();
  timeClient.update();

  unsigned long currentMillis = millis();
  // Check time for scheduled feeding
  int currentHour = hour(timeClient.getEpochTime());
  int currentMinute = minute(timeClient.getEpochTime());
  int currentSecond = second(timeClient.getEpochTime());

  if ((currentHour == webOnHour && currentMinute == webOnMinute && currentSecond == 0) ||
      (currentHour == webOffHour && currentMinute == webOffMinute && currentSecond == 0)) {
    activateServo();
  }

  // DHT sensor reading
  static unsigned long previousDHTMillis = 0;
  const long dhtInterval = 2000; // Interval to read DHT sensor
  if (currentMillis - previousDHTMillis >= dhtInterval) {
    previousDHTMillis = currentMillis;
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.print("%\t");
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println("°C");

      Firebase.setFloat(firebaseData, "/dht11/Temperature", temperature);
      Firebase.setFloat(firebaseData, "/dht11/Humidity", humidity);
    }
  }
  // Water sensor reading
  static unsigned long previousWaterMillis = 0;
  const long waterInterval = 2000; // Interval to read water sensor
  if (currentMillis - previousWaterMillis >= waterInterval) {
    previousWaterMillis = currentMillis;
    digitalWrite(POWER_PIN, HIGH);
    delay(10);
    int value = analogRead(SIGNAL_PIN);
    digitalWrite(POWER_PIN, LOW);

    Firebase.setFloat(fbdo, "/watersensor/WaterLevel", value);

    if (value < THRESHOLD) {
      Serial.println("Thiếu nước");
      digitalWrite(RELAY_PIN, HIGH);
      Firebase.setString(fbdo, "/watersensor/Status", "Bơm nước");
    } else {
      Serial.println("Đủ nước");
      digitalWrite(RELAY_PIN, LOW);
      Firebase.setString(fbdo, "/watersensor/Status", "Đủ nước");
    }
  }
}

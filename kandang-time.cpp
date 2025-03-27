#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define RELAY_KIPAS 14  
#define RELAY_LAMPU 12  
#define MQ135_PIN A0    

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);
ESP8266WebServer server(80);

char ssid[] = "nama wifi";
char pass[] = "password wifi";
String ipAddress;

const unsigned long waktuMasukKandang = 1742486400;
int tampilanLCD = 0;  
bool sensorError = false;

const char* bulanList[] = {"Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember"};

std::pair<int, int> getOptimalTemperature(int umur) {
    if (umur <= 7) return {32, 34};  
    if (umur <= 14) return {30, 32};  
    if (umur <= 21) return {28, 30};  
    if (umur <= 28) return {26, 28};  
    return {26, 31}; 
}

String getFormattedDate(unsigned long epochTime) {
    struct tm *timeInfo;
    time_t rawTime = epochTime;
    timeInfo = localtime(&rawTime);
    int day = timeInfo->tm_mday;
    int month = timeInfo->tm_mon;
    int year = timeInfo->tm_year + 1900;
    return String(day) + " " + bulanList[month] + " " + String(year);
}

float getAmoniaPPM() {
    int sensorValue = analogRead(MQ135_PIN);
    float voltage = sensorValue * (3.3 / 1024.0);
    float ppm = voltage * 10.0;
    return ppm;
}

float suhu = 0, kelembaban = 0, amonia = 0;

void kontrolSuhu() {
    int umur = ((timeClient.getEpochTime() - waktuMasukKandang) / 86400) + 1;
    auto suhuOptimal = getOptimalTemperature(umur);
    int jam = timeClient.getHours();
    bool bolehLampuNyala = umur <= 35 || (jam < 7 || jam >= 17);
    if (suhu < suhuOptimal.first && bolehLampuNyala) {
        digitalWrite(RELAY_LAMPU, HIGH);
        digitalWrite(RELAY_KIPAS, LOW);
    } else if (suhu > suhuOptimal.second || amonia >= 20) {
        digitalWrite(RELAY_KIPAS, HIGH);
        digitalWrite(RELAY_LAMPU, LOW);
    } else {
        digitalWrite(RELAY_KIPAS, LOW);
        digitalWrite(RELAY_LAMPU, LOW);
    }
}

void updateLCD() {
    lcd.clear();
    switch (tampilanLCD) {
        case 0:
            lcd.setCursor(0, 0); lcd.print("Tanggal:");
            lcd.setCursor(0, 1); lcd.print(getFormattedDate(timeClient.getEpochTime()));
            break;
        case 1:
            lcd.setCursor(0, 0); lcd.print("Jam:");
            lcd.setCursor(0, 1); lcd.print(timeClient.getFormattedTime());
            break;
        case 2:
            lcd.setCursor(0, 0); lcd.print("Masuk Kandang:");
            lcd.setCursor(0, 1); lcd.print(getFormattedDate(waktuMasukKandang));
            break;
        case 3:
            lcd.setCursor(0, 0); lcd.print("Umur Ayam:");
            lcd.setCursor(0, 1); lcd.print(((timeClient.getEpochTime() - waktuMasukKandang) / 86400) + 1); lcd.print(" hari");
            break;
        case 4:
            lcd.setCursor(0, 0); lcd.print("Suhu:");
            lcd.setCursor(0, 1);
            lcd.print(isnan(suhu) ? "Err" : String(suhu) + " C");
            break;
        case 5:
            lcd.setCursor(0, 0); lcd.print("Kelembaban:");
            lcd.setCursor(0, 1);
            lcd.print(isnan(kelembaban) ? "Err" : String(kelembaban) + " %");
            break;
        case 6:
            lcd.setCursor(0, 0); lcd.print("Amonia NH3:");
            lcd.setCursor(0, 1); lcd.print(amonia); lcd.print(" ppm");
            break;
        case 7:
            lcd.setCursor(0, 0); lcd.print("Lampu:");
            lcd.setCursor(0, 1); lcd.print(digitalRead(RELAY_LAMPU) ? "ON" : "OFF");
            break;
        case 8:
            lcd.setCursor(0, 0); lcd.print("Kipas:");
            lcd.setCursor(0, 1); lcd.print(digitalRead(RELAY_KIPAS) ? "ON" : "OFF");
            break;
        case 9:
            lcd.setCursor(0, 0); lcd.print("IP Address:");
            lcd.setCursor(0, 1); lcd.print(ipAddress);
            break;
    }
    delay(2000);
    tampilanLCD = (tampilanLCD + 1) % 10;
}

void handleRoot() {
    server.send(200, "text/html", "<html><body><h1>Monitoring Kandang</h1><p>Tanggal: " + getFormattedDate(timeClient.getEpochTime()) + "</p><p>Jam: " + timeClient.getFormattedTime() + "</p><p>Masuk Kandang: " + getFormattedDate(waktuMasukKandang) + "</p><p>Umur Ayam: " + String(((timeClient.getEpochTime() - waktuMasukKandang) / 86400) + 1) + " hari</p><p>Suhu: " + String(suhu) + " C</p><p>Kelembaban: " + String(kelembaban) + " %</p><p>Amonia NH3: " + String(amonia) + " ppm</p><p>Lampu: " + (digitalRead(RELAY_LAMPU) ? "ON" : "OFF") + "</p><p>Kipas: " + (digitalRead(RELAY_KIPAS) ? "ON" : "OFF") + "</p><p>IP Address: " + ipAddress + "</p></body></html>");
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }
    ipAddress = WiFi.localIP().toString();
    timeClient.begin();
    dht.begin();
    lcd.init();
    lcd.backlight();
    pinMode(RELAY_KIPAS, OUTPUT);
    pinMode(RELAY_LAMPU, OUTPUT);
    server.on("/", handleRoot);
    server.begin();
}

void loop() {
    ESP.wdtFeed();
    timeClient.update();
    server.handleClient();
    suhu = dht.readTemperature();
    kelembaban = dht.readHumidity();
    amonia = getAmoniaPPM();
    kontrolSuhu();
    updateLCD();
}

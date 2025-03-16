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

char ssid[] = "Nama_WiFi";
char pass[] = "Password_WiFi";
String ipAddress;

const unsigned long waktuMasukKandang = 1740530400;
String waktuMasukKandangStr;
int tampilanLCD = 0;  
bool sensorError = false;

const char* bulanList[] = {"Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember"};

float suhu, kelembaban, amonia;
int umurAyam;
String tanggal, waktuSekarang, statusLampu, statusKipas;

float getAmoniaPPM() {
    int sensorValue = analogRead(MQ135_PIN);
    float voltage = sensorValue * (3.3 / 1024.0);
    float ppm = voltage * 10.0;
    return ppm;
}

std::pair<int, int> getOptimalTemperature(int umur) {
    if (umur <= 7) return {32, 34};  
    if (umur <= 14) return {30, 32};  
    if (umur <= 21) return {28, 30};  
    if (umur <= 28) return {26, 28};  
    return {26, 31}; 
}

String getFormattedDate(unsigned long epochTime) {
    struct tm *timeinfo;
    time_t rawTime = epochTime;
    timeinfo = gmtime(&rawTime);

    int hari = timeinfo->tm_mday;
    int bulan = timeinfo->tm_mon + 1;
    int tahun = 1900 + timeinfo->tm_year;

    return String(hari) + " " + bulanList[bulan - 1] + " " + String(tahun);
}

void updateLCD() {
    lcd.clear();
    switch (tampilanLCD) {
        case 0: lcd.setCursor(0, 0); lcd.print("Tanggal:"); lcd.setCursor(0, 1); lcd.print(tanggal); break;
        case 1: lcd.setCursor(0, 0); lcd.print("Jam:"); lcd.setCursor(0, 1); lcd.print(waktuSekarang); break;
        case 2: lcd.setCursor(0, 0); lcd.print("Masuk Kandang:"); lcd.setCursor(0, 1); lcd.print(waktuMasukKandangStr); break;
        case 3: lcd.setCursor(0, 0); lcd.print("Umur Ayam:"); lcd.setCursor(0, 1); lcd.print(umurAyam); lcd.print(" hari"); break;
        case 4: lcd.setCursor(0, 0); lcd.print("Suhu:"); lcd.setCursor(0, 1); lcd.print(suhu); lcd.print(" C"); break;
        case 5: lcd.setCursor(0, 0); lcd.print("Kelembaban:"); lcd.setCursor(0, 1); lcd.print(kelembaban); lcd.print(" %"); break;
        case 6: lcd.setCursor(0, 0); lcd.print("Amonia NH3:"); lcd.setCursor(0, 1); lcd.print(amonia); lcd.print(" ppm"); break;
        case 7: lcd.setCursor(0, 0); lcd.print("Lampu:"); lcd.setCursor(0, 1); lcd.print(statusLampu); break;
        case 8: lcd.setCursor(0, 0); lcd.print("Kipas:"); lcd.setCursor(0, 1); lcd.print(statusKipas); break;
        default: lcd.setCursor(0, 0); lcd.print("IP Address:"); lcd.setCursor(0, 1); lcd.print(ipAddress); break;
    }
    tampilanLCD = (tampilanLCD + 1) % 10;
}

void bacaSensor() {
    timeClient.update();
    suhu = dht.readTemperature();
    kelembaban = dht.readHumidity();
    amonia = getAmoniaPPM();
    
    umurAyam = ((timeClient.getEpochTime() - waktuMasukKandang) / 86400) + 1;
    tanggal = getFormattedDate(timeClient.getEpochTime());
    waktuSekarang = timeClient.getFormattedTime();
    
    int jam = timeClient.getHours();
    auto suhuOptimal = getOptimalTemperature(umurAyam);
    
    bool bolehLampuNyala = umurAyam <= 35 || (jam < 7 || jam >= 17);
    
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
    
    statusLampu = digitalRead(RELAY_LAMPU) ? "ON" : "OFF";
    statusKipas = digitalRead(RELAY_KIPAS) ? "ON" : "OFF";
    
    updateLCD();
}

void handleRoot() {
    String html = "<html><head><title>Monitoring Kandang</title>";
    html += "<style>body{font-family:Arial;text-align:center;background:#f4f4f4;} h1{color:#333;} .status{font-size:18px;font-weight:bold;}</style></head><body>";

    html += "<h1>Monitoring Kandang Ayam 01</h1>";
    html += "<p><b>IP Address: </b>" + ipAddress + "</p>";
    html += "<p><b>Tanggal Sekarang: </b>" + tanggal + "</p>";
    html += "<p><b>Jam: </b>" + waktuSekarang + "</p>";
    html += "<p><b>Waktu Masuk Kandang: </b>" + waktuMasukKandangStr + "</p>";
    html += "<p><b>Umur Ayam: </b>" + String(umurAyam) + " hari</p>";
    html += "<p><b>Suhu: </b>" + String(suhu) + " °C</p>";
    html += "<p><b>Kelembaban: </b>" + String(kelembaban) + " %</p>";
    html += "<p><b>Amonia NH3: </b>" + String(amonia) + " ppm</p>";
    html += "<p class='status'><b>Lampu: </b>" + statusLampu + "</p>";
    html += "<p class='status'><b>Kipas: </b>" + statusKipas + "</p>";

    html += "</body></html>";
    server.send(200, "text/html", html);
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
    waktuMasukKandangStr = getFormattedDate(waktuMasukKandang);
    server.on("/", handleRoot);
    server.begin();
}

void loop() {
    bacaSensor();
    server.handleClient();
    delay(2000);
}

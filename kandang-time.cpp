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
int tampilanLCD = 0;  
bool sensorError = false;

const char* bulanList[] = {"Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember"};

float suhu, kelembaban, amonia;
String tanggal, waktuSekarang;
int umurAyam;
String statusLampu, statusKipas;

float getAmoniaPPM() {
    int sensorValue = analogRead(MQ135_PIN);
    float voltage = sensorValue * (3.3 / 1024.0);
    return voltage * 10.0;
}

std::pair<int, int> getOptimalTemperature(int umur) {
    if (umur <= 7) return {32, 34};  
    if (umur <= 14) return {30, 32};  
    if (umur <= 21) return {28, 30};  
    if (umur <= 28) return {26, 28};  
    return {26, 31}; 
}

void bacaSensor() {
    timeClient.update();
    kelembaban = dht.readHumidity();
    suhu = dht.readTemperature();
    amonia = getAmoniaPPM();

    time_t epochTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&epochTime);
    tanggal = String(timeInfo->tm_mday) + " " + bulanList[timeInfo->tm_mon] + " " + String(timeInfo->tm_year + 1900);
    waktuSekarang = timeClient.getFormattedTime();

    if (isnan(kelembaban) || isnan(suhu)) {
        sensorError = true;
        Serial.println("⚠ Sensor Gagal Membaca Data!");
        return;
    } else {
        sensorError = false;
    }

    umurAyam = ((epochTime - waktuMasukKandang) / 86400) + 1;
    int jam = timeClient.getHours();
    auto suhuOptimal = getOptimalTemperature(umurAyam);
    int suhu_min = suhuOptimal.first;
    int suhu_max = suhuOptimal.second;

    bool bolehLampuNyala = umurAyam <= 35 || (jam < 7 || jam >= 17);

    if (suhu < suhu_min && bolehLampuNyala) {
        digitalWrite(RELAY_LAMPU, HIGH);
        digitalWrite(RELAY_KIPAS, LOW);
    } else if (suhu > suhu_max || amonia >= 20) {
        digitalWrite(RELAY_KIPAS, HIGH);
        digitalWrite(RELAY_LAMPU, LOW);
    } else {
        digitalWrite(RELAY_KIPAS, LOW);
        digitalWrite(RELAY_LAMPU, LOW);
    }

    statusLampu = digitalRead(RELAY_LAMPU) ? "ON" : "OFF";
    statusKipas = digitalRead(RELAY_KIPAS) ? "ON" : "OFF";
}

void updateLCD() {
    lcd.clear();
    switch (tampilanLCD) {
        case 0:
            lcd.setCursor(0, 0); lcd.print("Tanggal:");
            lcd.setCursor(0, 1); lcd.print(tanggal);
            break;
        case 1:
            lcd.setCursor(0, 0); lcd.print("Jam:");
            lcd.setCursor(0, 1); lcd.print(waktuSekarang);
            break;
        case 2:
            lcd.setCursor(0, 0); lcd.print("Umur Ayam:");
            lcd.setCursor(0, 1); lcd.print(umurAyam); lcd.print(" hari");
            break;
        case 3:
            lcd.setCursor(0, 0); lcd.print("Suhu:");
            lcd.setCursor(0, 1); lcd.print(suhu); lcd.print(" C");
            break;
        case 4:
            lcd.setCursor(0, 0); lcd.print("Kelembaban:");
            lcd.setCursor(0, 1); lcd.print(kelembaban); lcd.print(" %");
            break;
        case 5:
            lcd.setCursor(0, 0); lcd.print("Amonia NH3:");
            lcd.setCursor(0, 1); lcd.print(amonia); lcd.print(" ppm");
            break;
        case 6:
            lcd.setCursor(0, 0); lcd.print("Lampu:");
            lcd.setCursor(0, 1); lcd.print(statusLampu);
            break;
        case 7:
            lcd.setCursor(0, 0); lcd.print("Kipas:");
            lcd.setCursor(0, 1); lcd.print(statusKipas);
            break;
        default:
            lcd.setCursor(0, 0); lcd.print("IP Address:");
            lcd.setCursor(0, 1); lcd.print(ipAddress);
            break;
    }
    tampilanLCD = (tampilanLCD + 1) % 9;
}

void handleRoot() {
    String html = "<html><head><title>Monitoring Kandang</title>";
    html += "<style>body{font-family:Arial;text-align:center;background:#f4f4f4;}";
    html += "h1{color:#333;} .status{font-size:18px;font-weight:bold;}";
    html += ".error{color:red;}</style></head><body>";

    html += "<h1>Monitoring Kandang Ayam 01</h1>";
    html += "<p><b>IP Address: </b>" + ipAddress + "</p>";
    html += "<p><b>Tanggal: </b>" + tanggal + "</p>";
    html += "<p><b>Jam: </b>" + waktuSekarang + "</p>";

    if (sensorError) {
        html += "<p class='error'>⚠ Sensor Gagal Membaca Data!</p>";
    } else {
        html += "<p><b>Umur Ayam: </b>" + String(umurAyam) + " hari</p>";
        html += "<p><b>Suhu: </b>" + String(suhu) + " °C</p>";
        html += "<p><b>Kelembaban: </b>" + String(kelembaban) + " %</p>";
        html += "<p><b>Amonia NH3: </b>" + String(amonia) + " ppm</p>";
    }

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

    server.on("/", handleRoot);
    server.begin();
}

void loop() {
    bacaSensor();
    updateLCD();
    server.handleClient();
    delay(2000);
}

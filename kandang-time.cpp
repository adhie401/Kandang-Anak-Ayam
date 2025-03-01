#include <ESP8266WiFi.h>
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
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);

char ssid[] = "nama wifi";
char pass[] = "password wifi";

const unsigned long waktuMasukKandang = 1740530400;

int tampilanLCD = 0;  

const char* bulanList[] = {"Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember"};

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
    if (umur <= 35) return {24, 26};  
    if (umur <= 42) return {22, 24};  
    if (umur <= 56) return {20, 22};  
    return {18, 22}; 
}

void bacaSensor() {
    timeClient.update();
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    float amonia = getAmoniaPPM();
    String waktuSekarang = timeClient.getFormattedTime();
    unsigned long waktuSekarangUnix = timeClient.getEpochTime();
    int umurAyam = ((waktuSekarangUnix - waktuMasukKandang) / 86400) + 1;
    
    struct tm *timeinfo;
    time_t rawtime = waktuSekarangUnix;
    timeinfo = localtime(&rawtime);
    char tanggalSekarang[20];
    sprintf(tanggalSekarang, "%d %s %d", timeinfo->tm_mday, bulanList[timeinfo->tm_mon], timeinfo->tm_year + 1900);

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Gagal membaca sensor DHT22!");
        return;
    }

    auto suhuOptimal = getOptimalTemperature(umurAyam);
    int suhu_min = suhuOptimal.first;
    int suhu_max = suhuOptimal.second;

    if (temperature <= suhu_max) {
        digitalWrite(RELAY_LAMPU, LOW);
        Serial.println("Lampu: OFF");
    } 
    if (temperature < suhu_min) {
        digitalWrite(RELAY_LAMPU, HIGH);
        Serial.println("Lampu: ON");
    }

    if (temperature > suhu_max || amonia >= 20) {
        digitalWrite(RELAY_KIPAS, HIGH);
        Serial.println("Kipas: ON");
    } else {
        digitalWrite(RELAY_KIPAS, LOW);
        Serial.println("Kipas: OFF");
    }

    Serial.print("Waktu: "); Serial.print(waktuSekarang);
    Serial.print(" | Tanggal: "); Serial.print(tanggalSekarang);
    Serial.print(" | Umur Ayam: "); Serial.print(umurAyam); Serial.print(" hari");
    Serial.print(" | Suhu: "); Serial.print(temperature); Serial.print("°C");
    Serial.print(" | Kelembaban: "); Serial.print(humidity); Serial.print("%");
    Serial.print(" | NH3: "); Serial.print(amonia); Serial.println(" ppm");

    lcd.clear();
    switch (tampilanLCD) {
        case 0:
            lcd.setCursor(0, 0);
            lcd.print("Umur Ayam:");
            lcd.setCursor(0, 1);
            lcd.print(umurAyam);
            lcd.print(" Hari");
            break;
        case 1:
            lcd.setCursor(0, 0);
            lcd.print("Tanggal:");
            lcd.setCursor(0, 1);
            lcd.print(tanggalSekarang);
            break;
        case 2:
            lcd.setCursor(0, 0);
            lcd.print("Jam:");
            lcd.setCursor(0, 1);
            lcd.print(waktuSekarang);
            break;
        case 3:
            lcd.setCursor(0, 0);
            lcd.print("Suhu:");
            lcd.setCursor(0, 1);
            lcd.print(temperature);
            lcd.print(" C");
            break;
        case 4:
            lcd.setCursor(0, 0);
            lcd.print("Kelembaban:");
            lcd.setCursor(0, 1);
            lcd.print(humidity);
            lcd.print(" %");
            break;
        case 5:
            lcd.setCursor(0, 0);
            lcd.print("NH3:");
            lcd.setCursor(0, 1);
            lcd.print(amonia);
            lcd.print(" ppm");
            break;
    }

    tampilanLCD = (tampilanLCD + 1) % 6;
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }

    timeClient.begin();
    dht.begin();
    lcd.init();
    lcd.backlight();
    lcd.clear();
    
    pinMode(RELAY_KIPAS, OUTPUT);
    pinMode(RELAY_LAMPU, OUTPUT);

    Serial.println("Sistem Siap!");
}

void loop() {
    bacaSensor();
    delay(2000);
}

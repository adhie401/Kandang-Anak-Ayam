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
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

char ssid[] = "Hisyam";
char pass[] = "#Hisyam1111";

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
    return {24, 31}; 
}

void updateLCD(int tampilan, int umurAyam, float temperature, float humidity, float amonia, String waktuSekarang, String tanggalSekarang, String tanggalMasuk) {
    lcd.clear();
    if (tampilan == 0) {
        lcd.setCursor(0, 0);
        lcd.print("Tanggal:");
        lcd.setCursor(0, 1);
        lcd.print(tanggalSekarang);
    } else if (tampilan == 1) {
        lcd.setCursor(0, 0);
        lcd.print("Umur Ayam:");
        lcd.setCursor(0, 1);
        lcd.print(umurAyam);
        lcd.print(" hari");
    } else if (tampilan == 2) {
        lcd.setCursor(0, 0);
        lcd.print("Masuk Kandang:");
        lcd.setCursor(0, 1);
        lcd.print(tanggalMasuk);
    } else if (tampilan == 3) {
        lcd.setCursor(0, 0);
        lcd.print("Suhu:");
        lcd.setCursor(0, 1);
        lcd.print(temperature);
        lcd.print(" C");
    } else if (tampilan == 4) {
        lcd.setCursor(0, 0);
        lcd.print("Kelembaban:");
        lcd.setCursor(0, 1);
        lcd.print(humidity);
        lcd.print(" %");
    } else if (tampilan == 5) {
        lcd.setCursor(0, 0);
        lcd.print("Amonia NH3:");
        lcd.setCursor(0, 1);
        lcd.print(amonia);
        lcd.print(" ppm");
    } else {
        lcd.setCursor(0, 0);
        lcd.print("Waktu:");
        lcd.setCursor(0, 1);
        lcd.print(waktuSekarang);
    }
}

void bacaSensor() {
    timeClient.update();
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    float amonia = getAmoniaPPM();
    String waktuSekarang = timeClient.getFormattedTime();
    unsigned long waktuSekarangUnix = timeClient.getEpochTime();

    time_t rawtime = waktuSekarangUnix + (7 * 3600);
    struct tm *timeinfo = localtime(&rawtime);
    char tanggalSekarang[20];
    sprintf(tanggalSekarang, "%d %s %d", timeinfo->tm_mday, bulanList[timeinfo->tm_mon], timeinfo->tm_year + 1900);

    time_t rawMasuk = waktuMasukKandang + (7 * 3600);
    struct tm *timeMasukInfo = localtime(&rawMasuk);
    char tanggalMasuk[20];
    sprintf(tanggalMasuk, "%d %s %d", timeMasukInfo->tm_mday, bulanList[timeMasukInfo->tm_mon], timeMasukInfo->tm_year + 1900);

    int umurAyam = (waktuSekarangUnix - waktuMasukKandang) / 86400;
    if ((waktuSekarangUnix % 86400) < (waktuMasukKandang % 86400)) {
        umurAyam--;
    }
    
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Gagal membaca sensor DHT22!");
        return;
    }

    auto suhuOptimal = getOptimalTemperature(umurAyam);
    int suhu_min = suhuOptimal.first;
    int suhu_max = suhuOptimal.second;

    if (temperature < suhu_min) {
        digitalWrite(RELAY_LAMPU, HIGH);
        digitalWrite(RELAY_KIPAS, LOW);
    } else if (temperature > suhu_max || amonia >= 20) {
        digitalWrite(RELAY_KIPAS, HIGH);
        digitalWrite(RELAY_LAMPU, LOW);
    } else {
        digitalWrite(RELAY_KIPAS, LOW);
        digitalWrite(RELAY_LAMPU, LOW);
    }
    umurAyam = umurAyam + 1;
    Serial.print("Waktu: "); Serial.print(waktuSekarang);
    Serial.print(" | Tanggal: "); Serial.print(tanggalSekarang);
    Serial.print(" | Umur Ayam: "); Serial.print(umurAyam); Serial.print(" hari");
    Serial.print(" | Suhu: "); Serial.print(temperature); Serial.print("°C");
    Serial.print(" | Kelembaban: "); Serial.print(humidity); Serial.print("%");
    Serial.print(" | NH3: "); Serial.print(amonia); Serial.println(" ppm");

    updateLCD(tampilanLCD, umurAyam, temperature, humidity, amonia, waktuSekarang, tanggalSekarang, tanggalMasuk);
    tampilanLCD = (tampilanLCD + 1) % 7;
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
}

void loop() {
    bacaSensor();
    delay(2000);
}

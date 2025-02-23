#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi sensor & modul
#define DHTPIN 2
#define DHTTYPE DHT22
#define RELAY_HEATER 3   // Relay pemanas (aktif HIGH)
#define MQ135_PIN A0     // Sensor MQ-135
#define RELAY_FAN 4      // Relay kipas (aktif HIGH)
#define THRESHOLD_NH3 10   // Batas aman NH3 dalam ppm
#define THRESHOLD_CO2 3000 // Batas aman CO2 dalam ppm

RTC_DS3231 rtc;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C 0x27 atau 0x3F

// Atur tanggal mulai ayam menetas
const int tahun_mulai = 2025;
const int bulan_mulai = 2;
const int tanggal_mulai = 24;

// Suhu terendah yang diizinkan
const float SUHU_MINIMUM = 26.0;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    dht.begin();
    lcd.begin();
    lcd.backlight();
    
    pinMode(RELAY_HEATER, OUTPUT);
    pinMode(RELAY_FAN, OUTPUT);
    digitalWrite(RELAY_HEATER, LOW); // Pemanas mati awal
    digitalWrite(RELAY_FAN, LOW);    // Kipas mati awal

    if (!rtc.begin()) {
        Serial.println("RTC tidak terdeteksi!");
        while (1);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC kehilangan daya, set ulang waktu!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set waktu sesuai komputer saat upload
    }

    lcd.setCursor(0, 0);
    lcd.print("Sistem Siap...");
    delay(2000);
}

void loop() {
    DateTime now = rtc.now();
    DateTime startDate(tahun_mulai, bulan_mulai, tanggal_mulai, 0, 0, 0);
    int daysElapsed = (now.unixtime() - startDate.unixtime()) / 86400;

    // **Baca sensor suhu**
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Gagal membaca sensor DHT!");
        return;
    }

    // **Baca sensor gas MQ-135**
    int sensorValue = analogRead(MQ135_PIN);
    float voltage = sensorValue * (5.0 / 1023.0); // Konversi ke Volt
    float ppm_NH3 = voltage * 50.0;  // Estimasi NH3
    float ppm_CO2 = voltage * 1000.0; // Estimasi CO2

    // **Hitung suhu optimal**
    float minTemp, maxTemp;
    getSuhuOptimal(daysElapsed, minTemp, maxTemp);
    if (minTemp < SUHU_MINIMUM) minTemp = SUHU_MINIMUM;

    // **Kontrol pemanas (aktif HIGH)**
    bool heaterOn = false;
    if (temperature < minTemp) {
        digitalWrite(RELAY_HEATER, HIGH);
        heaterOn = true;
    } else if (temperature > maxTemp) {
        digitalWrite(RELAY_HEATER, LOW);
        heaterOn = false;
    }

    // **Kontrol kipas (aktif HIGH)**
    bool fanOn = false;
    if (ppm_NH3 > THRESHOLD_NH3 || ppm_CO2 > THRESHOLD_CO2) {
        digitalWrite(RELAY_FAN, HIGH);
        fanOn = true;
    } else {
        digitalWrite(RELAY_FAN, LOW);
        fanOn = false;
    }

    // **Tampilkan di Serial Monitor**
    Serial.print("Hari ke-"); Serial.print(daysElapsed);
    Serial.print(" | Suhu: "); Serial.print(temperature);
    Serial.print(" °C | NH3: "); Serial.print(ppm_NH3);
    Serial.print(" ppm | CO2: "); Serial.print(ppm_CO2);
    Serial.print(" ppm | Pemanas: ");
    Serial.print(heaterOn ? "ON" : "OFF");
    Serial.print(" | Kipas: ");
    Serial.println(fanOn ? "ON" : "OFF");

    // **Tampilkan di LCD**
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hari: "); lcd.print(daysElapsed);
    
    lcd.setCursor(0, 1);
    lcd.print("Suhu: "); lcd.print(temperature);
    lcd.print((char)223); lcd.print("C");

    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NH3: "); lcd.print(ppm_NH3);
    lcd.print("ppm");

    lcd.setCursor(0, 1);
    lcd.print("CO2: "); lcd.print(ppm_CO2);
    lcd.print("ppm");

    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pemanas: ");
    lcd.print(heaterOn ? "ON " : "OFF");

    lcd.setCursor(0, 1);
    lcd.print("Kipas: ");
    lcd.print(fanOn ? "ON " : "OFF");

    delay(2000);
}

// **Fungsi menentukan suhu optimal berdasarkan umur ayam**
void getSuhuOptimal(int hari, float &minTemp, float &maxTemp) {
    if (hari <= 7)       { minTemp = 32; maxTemp = 34; }
    else if (hari <= 14) { minTemp = 30; maxTemp = 32; }
    else if (hari <= 21) { minTemp = 28; maxTemp = 30; }
    else if (hari <= 28) { minTemp = 26; maxTemp = 28; }
    else                 { minTemp = 26; maxTemp = 28; }
}

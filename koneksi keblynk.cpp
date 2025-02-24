#define BLYNK_TEMPLATE_ID "template_id"
#define BLYNK_TEMPLATE_NAME "template-name"
#define BLYNK_AUTH_TOKEN "your-token"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h>

char ssid[] = "your-ssid";
char pass[] = "your-wifi-password";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);

#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

BlynkTimer timer;

void bacaSensor() {
    timeClient.update();
    String formattedTime = timeClient.getFormattedTime();

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("❌ Gagal membaca sensor DHT22!");
        return;
    }

    Serial.print("🕒 Waktu: "); Serial.print(formattedTime);
    Serial.print(" | 🌡 Suhu: "); Serial.print(temperature);
    Serial.print("°C | 💧 Kelembaban: "); Serial.print(humidity);
    Serial.println("%");

    Blynk.virtualWrite(V1, temperature);
    Blynk.virtualWrite(V2, humidity);
}

BLYNK_CONNECTED() {
    Serial.println("✅ Tersambung ke Blynk Cloud!");
}

void setup() {
    Serial.begin(115200);
    
    WiFi.begin(ssid, pass);
    Serial.print("🔌 Menghubungkan ke WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi Terhubung!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    timeClient.begin();

    dht.begin();

    timer.setInterval(5000L, bacaSensor);
}

void loop() {
    Blynk.run();
    timer.run();
}

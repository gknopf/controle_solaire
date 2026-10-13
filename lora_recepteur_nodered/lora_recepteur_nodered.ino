

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"


// --- Configuration LoRa ---
const int LORA_CS = 5;
const int LORA_RST = 14;
const int LORA_DIO0 = 2;
const long LORA_FREQ = 868E6;

// --- Structure des données ---
#pragma pack(push, 1)
typedef struct {
  uint8_t sensorId;
  int32_t tension;
  int32_t courant;
  uint8_t checksum;
} SensorData;
#pragma pack(pop)

uint8_t calculateChecksum(SensorData *data) {
  return (uint8_t)((data->sensorId ^ data->tension ^ data->courant) &0xFF);
} 



// --- Configuration MQTT ---
const char* mqtt_server = "192.168.1.140"; // Adresse IP de ton broker Mosquitto
const int mqtt_port = 1883;
const char* mqtt_topic = "lora/data";
const char* mqtt_client_id = "recepteur_lora";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastMqttAttempt = 0;
const unsigned long MQTT_RETRY_MS = 5000;
int mqttFailCount = 0;




void setupWiFi() {
  Serial.println("\nConnexion au Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Wi-Fi connecté !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

// --- Reconnexion MQTT NON bloquante : une tentative max toutes les 5 s ---
void maintainMQTT() {
  if (mqttClient.connected()) return;

  if (millis() - lastMqttAttempt < MQTT_RETRY_MS) return;  // pas maintenant
  lastMqttAttempt = millis();

  Serial.println("Tentative de connexion au broker MQTT...");
  if (mqttClient.connect(mqtt_client_id)) {
    Serial.println("Connecte au broker MQTT !");
    mqttFailCount = 0;
  } else {
    mqttFailCount++;
    Serial.printf("Echec MQTT (code %d, tentative #%d)\n",
                  mqttClient.state(), mqttFailCount);
    // le loop() continue : les paquets LoRa restent traites
  }
}

void publishToMQTT(SensorData *data, int rssi, float snr) {
  if (!mqttClient.connected()) return;   // pas de tentative ici : maintainMQTT s'en charge

  char output[200];
  JsonDocument doc;
  doc["sensorid"] = data->sensorId;
  doc["tension"]  = data->tension;     // tension mV
  doc["courant"]  = data->courant;     // courant mA
  doc["rssi"]     = rssi;
  doc["snr"]      = snr;
  serializeJson(doc, output);

  if (mqttClient.publish(mqtt_topic, output)) {
    Serial.println("Donnees publiees sur MQTT");
  } else {
    Serial.println("Echec de publication MQTT");
  }
}



void setup() {
  Serial.begin(115200);
  Serial.println("\n[Récepteur LoRa + MQTT]");

  // Initialisation LoRa
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("❌ LoRa non initialisé !");
    while (1) delay(1000);
  }
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  Serial.println("✅ LoRa prêt à recevoir");

  // Connexion Wi-Fi
  setupWiFi();

  // Configuration MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setKeepAlive(60);
  // premiere connexion immediate (lastMqttAttempt = 0)
  maintainMQTT();
}

void loop() {
  // --- MQTT sans jamais bloquer ---
  maintainMQTT();
  mqttClient.loop();

  // --- Reception LoRa ---
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {

    if (packetSize == sizeof(SensorData)) {
      SensorData receivedData;
      LoRa.readBytes((uint8_t *)&receivedData, sizeof(receivedData));

      // Checksum applicatif : rejet si invalide
      uint8_t expected = calculateChecksum(&receivedData);
      if (receivedData.checksum != expected) {
        Serial.printf("Checksum LoRa invalide: recu=%d attendu=%d (rejet)\n",
                      receivedData.checksum, expected);
        return;   // rien ne part sur MQTT
      }

      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();

      Serial.printf("[LoRa] Capteur %d | tension=%d mV | courant=%d mA",
                    receivedData.sensorId, receivedData.tension, receivedData.courant);
      Serial.printf(" | RSSI: %d dBm | SNR: %.1f dB\n", rssi, snr);

      publishToMQTT(&receivedData, rssi, snr);

    } else {
      // Taille inattendue : vider le FIFO pour ne pas polluer le paquet suivant
      Serial.printf("Paquet LoRa de taille inattendue: %d (ignore)\n", packetSize);
      while (LoRa.available()) LoRa.read();
    }
  }
}

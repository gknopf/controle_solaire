

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Configuration LoRa ---
const int LORA_CS = 5;
const int LORA_RST = 14;
const int LORA_DIO0 = 2;
const long LORA_FREQ = 868E6;

// --- Structure des données ---
#pragma pack(push, 1)
typedef struct {
  uint8_t sensorId;
  int a;
  float b;
  char c;
  uint8_t checksum;
} SensorData;
#pragma pack(pop)

uint8_t calculateChecksum(SensorData *data) {
  return (uint8_t)((data->sensorId ^ data->a ^ (int)(data->b * 100) ^ data->c) & 0xFF);
}

// --- Configuration Wi-Fi ---
const char* wifi_ssid = "knobuntufree";
const char* wifi_password = "Pech_Vogel_free123";

// --- Configuration MQTT ---
const char* mqtt_server = "192.168.1.140"; // Adresse IP de ton broker Mosquitto
const int mqtt_port = 1883;
const char* mqtt_topic = "lora/data";
const char* mqtt_client_id = "recepteur_lora";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// --- Fonction pour se connecter au Wi-Fi ---
void setupWiFi() {
  Serial.println("\nConnexion au Wi-Fi...");
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Wi-Fi connecté !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

// --- Fonction pour se reconnecter à MQTT ---
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.println("Tentative de connexion au broker MQTT...");
    if (mqttClient.connect(mqtt_client_id)) {
      Serial.println("✅ Connecté au broker MQTT !");
    } else {
      Serial.print("❌ Échec de connexion MQTT (code : ");
      Serial.print(mqttClient.state());
      Serial.println("). Nouvelle tentative dans 5 secondes...");
      delay(5000);
    }
  }
}

// --- Fonction pour publier les données sur MQTT ---
void publishToMQTT(SensorData *data, int rssi, float snr) {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }

  // Créer un JSON pour les données (facultatif, mais utile pour Node-RED)
  String payload = "{";
  payload += "\"sensorId\":" + String(data->sensorId) + ",";
  payload += "\"a\":" + String(data->a) + ",";
  payload += "\"b\":" + String(data->b) + ",";
  payload += "\"c\":\"" + String(data->c) + "\",";
  payload += "\"rssi\":" + String(rssi) + ",";
  payload += "\"snr\":" + String(snr);
  payload += "}";

  // Publier sur le topic MQTT
  if (mqttClient.publish(mqtt_topic, payload.c_str())) {
    Serial.println("✅ Données publiées sur MQTT !");
  } else {
    Serial.println("❌ Échec de publication MQTT !");
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
  reconnectMQTT();
}

void loop() {
  // Gérer la connexion MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Lire les données LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize >= sizeof(SensorData)) {
    SensorData receivedData;
    LoRa.readBytes((uint8_t*)&receivedData, sizeof(receivedData));

    // Vérifier le checksum
    uint8_t expectedChecksum = calculateChecksum(&receivedData);
    if (receivedData.checksum != expectedChecksum) {
      Serial.println("❌ Checksum invalide (LoRa) !");
      return;
    }

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    Serial.printf("✅ [LoRa] Capteur %d | a=%d | b=%.2f | c=%c\n",
                  receivedData.sensorId, receivedData.a,
                  receivedData.b, receivedData.c);
    Serial.printf("   RSSI: %d dBm | SNR: %.1f dB\n\n", rssi, snr);

    // Publier les données sur MQTT
    publishToMQTT(&receivedData, rssi, snr);
  }
}

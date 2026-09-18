#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>

// Broches I2C pour l'ADS1115 (fixées sur le PCB)
#define sdapin 10
#define sclpin 3

// Adresse MAC du récepteur
uint8_t broadcastAddress[] = {0xe0, 0x72, 0xa1, 0x72, 0x33, 0xbc};

// Structure des données
typedef struct struct_message {
  uint8_t device_id;
  uint32_t tensionCC;  // Tension en mV
  uint32_t courant;    // Courant en mA
  uint8_t checksum;
} struct_message;

struct_message myData;


uint8_t calculateChecksum(struct_message *data){
  return (uint8_t)((data->device_id ^ data->tensionCC ^ data->courant) &0xFF);
}



Adafruit_ADS1115 ads;

// Paramètres du capteur de tension
const int voltagePin = 4;       // Broche ADC pour la tension batterie
const float tensionConversionSlope = 0.0182292;
const float tensionConversionOffset = -0.005208;

// Paramètres du shunt et de l'ADS1115
const float shuntSensitivity = 0.00075; // 0.75 mV/A
const float gain = 16.0; // Gain de l'ADS1115

esp_now_peer_info_t peerInfo;

// Callback pour l'envoi ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nStatut envoi: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Succès" : "Échec");
}

void setup() {
  Serial.begin(115200);

  // Configuration Wi-Fi
  WiFi.mode(WIFI_STA);

  // Initialisation ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur initialisation ESP-NOW");
    return;
  }

  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));

  // Configuration du pair (récepteur)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Échec ajout du pair");
    return;
  }

  // Initialisation de Wire avec les broches personnalisées
  Wire.begin(sdapin, sclpin);

  // Initialisation de l'ADS1115
  if (!ads.begin()) {
    Serial.println("Échec initialisation ADS1115");
    while (1);
  }
  ads.setGain(GAIN_SIXTEEN); // Gain 16x pour ±0.256V

  Serial.println("ADS1115 initialisé avec succès");
  myData.device_id = 1; // ID de l'appareil
}

void loop() {
  // Lecture du courant via ADS1115 (A0 - A1)
  int16_t courantAdc = ads.readADC_Differential_0_1();
  float voltage = (courantAdc * 4.096) / (32767.0 * gain); // Conversion en volts
  float current = voltage / shuntSensitivity; // Calcul du courant en A
  myData.courant = static_cast<int32_t>(current * 1000); // Conversion en mA

  // Lecture de la tension batterie
  int voltageAdc = analogRead(voltagePin);
  float batteryVoltage = tensionConversionSlope * voltageAdc + tensionConversionOffset;
  myData.tensionCC = static_cast<int32_t>(batteryVoltage * 1000); // Conversion en mV
  myData.checksum=calculateChecksum(&myData);
  // Affichage pour débogage
  Serial.print("Tension batterie: ");
  Serial.print(batteryVoltage, 3);
  Serial.print(" V, Courant: ");
  Serial.print(current, 3);
  Serial.println(" A");

  // Envoi des données via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  if (result == ESP_OK) {
    Serial.println("Données envoyées avec succès");
  } else {
    Serial.println("Échec envoi des données");
  }

  delay(2000); // Délai entre les envois
}

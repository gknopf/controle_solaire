#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_ADS1X15.h>

#define sdapin 10
#define sclpin 3

uint8_t broadcastAddress[] = {0xe0, 0x72, 0xa1, 0x72, 0x33, 0xbc};
// --- Structure des données ---

typedef struct struct_message {
  uint8_t device_id;
  uint32_t tensionCC;
  uint32_t courant;
} struct_message;

// Create a struct_message called myData
struct_message myData;

//mesure tension
const int voltagePin = 4;      // Broche ADC (ex: GPIO4)
const float Vref = 3.3;        // Tension de référence ADC
const int adcResolution = 4095; // Résolution 12 bits
//simulation qucs
//44v->1.94 58v->2.56v   y=0.018229*voltread-0.05208
const float R1 = 940000.0;   // 940k
const float R2 = 300000.0;    // 300kΩ
const float R3 = 22000.0;    // 220kΩ
const float R4 = 100000.0;    // 100kΩ
// Initialisation de l'ADS1115

Adafruit_ADS1115 ads;

// Paramètres du shunt
const float shuntSensitivity = 0.00075; // 75 mV / 100A = 0.75 mV/A = 0.00075 V/A
const float gain = 16.0; // Gain de l'ADS1115 (16x pour mesurer 75 mV avec une résolution optimale)


esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur initialisation ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

// Initialisation de l'ADS1115
  Wire.end();
  Wire.setPins(sdapin,sclpin);
  Wire.begin();
  if (!ads.begin()) {
    Serial.println("Échec de l'initialisation de l'ADS1115");
    while (1);
  }
  // Configuration du gain (16x pour ±0.256V, idéal pour 75 mV)
  ads.setGain(GAIN_SIXTEEN);

  Serial.println("ADS1115 initialisé");
 myData.device_id=1;



  
}
 
void loop() {
   // Lecture de la tension différentielle (A0 - A1)
  int16_t courantAdc = ads.readADC_Differential_0_1();
  float voltage = (courantAdc * 4.096) / (32767 * gain); // Conversion en volts (4.096V = tension max pour gain=16)

  // Calcul du courant
  float current = voltage / shuntSensitivity;
   myData.courant== static_cast<int32_t>(current * 1000); //en mA

//tension batterie

  int voltageAdc = analogRead(voltagePin);
  float batteryVoltage=0.0182292*voltageAdc-0.005208;
  myData.tensionCC== static_cast<int32_t>(batteryVoltage * 1000); //en mmV
  
  
 
  
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  delay(2000);
}

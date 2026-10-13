#include <esp_now.h>
#include <WiFi.h>



// Adresse MAC du récepteur
uint8_t broadcastAddress[] = {0xe0, 0x72, 0xa1, 0x72, 0x34, 0xec};

// Structure des données

#pragma pack(push,1)
typedef struct struct_message {
  uint8_t device_id;
  int32_t tensionCC;  // Tension en mV
  int32_t courant;    // Courant en mA
  uint8_t checksum;
} struct_message;
#pragma pack(pop)
struct_message myData;

uint8_t calculateChecksum(struct_message *data){
  return (uint8_t)((data->device_id ^ data->tensionCC ^ data->courant) &0xFF);
}

// Paramètres du capteur de tension
const int voltagePin = 4;       // Broche ADC pour la tension batterie
const float tensionConversionSlope = 0.0182292;
const float tensionConversionOffset = -0.005208;
const float coef_ajust_voltage =0.911;



// Paramètres du capteur de courant
const int courantPin = 3;       // Broche ADC pour lle courant batterie

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

  
  analogSetPinAttenuation(voltagePin, ADC_11db);
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));

  // Configuration du pair (récepteur)
 memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Échec ajout du pair");
    return;
  }

 
  myData.device_id = 1; // ID de l'appareil
}

void loop() {
 

  // Lecture ducourantbatterie
  int courantAdc = analogRead(courantPin);
  //envoi du courant : la conversion en ampere se fait dans nodered fonction shunt.
  myData.courant=courantAdc;
  
  //lecture de la tension batterie
   int voltageAdc = analogRead(voltagePin);
  float batteryVoltage =coef_ajust_voltage*( tensionConversionSlope * voltageAdc + tensionConversionOffset);
  myData.tensionCC = static_cast<int32_t>(batteryVoltage * 1000); // Conversion en mV
  
  myData.checksum=calculateChecksum(&myData);
  myData.checksum=calculateChecksum(&myData);
  
  // Affichage pour débogage
  
  Serial.print("Tension batterie: ");
  Serial.print(batteryVoltage, 3);
  Serial.print(" V, Courant: ");
  Serial.print(" Courant: ");
  Serial.print(courantAdc);
  Serial.println(" bits");

  // Envoi des données via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  if (result == ESP_OK) {
    Serial.println("Données envoyées avec succès");
  } else {
    Serial.println("Échec envoi des données");
  }

  delay(10000); // Délai entre les envois
}

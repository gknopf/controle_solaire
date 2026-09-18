

#include <esp_now.h>
#include <WiFi.h>
#include <HardwareSerial.h>


// Structure reception donnees
// meme structure que l'emmeteur
typedef struct struct_message {
  uint8_t device_id;
  uint32_t tensionCC;
  uint32_t courant;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("device_id: ");
  Serial.println(myData.device_id);
  Serial.print("tensionCC: ");
  Serial.println(myData.tensionCC);
  Serial.print("courant ");
  Serial.println(myData.courant);
 
  Serial.println();
}
 
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}
 
void loop() {

}

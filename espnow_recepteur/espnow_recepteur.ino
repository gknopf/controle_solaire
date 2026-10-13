#include <esp_now.h>
#include <WiFi.h>
#include <HardwareSerial.h>

HardwareSerial SerialPort(1); // uart 1 gpio 20 RX gpio21 TX pour esp32-c3
const int BAUD_RATE =115200;

// Structure reception donnees
// meme structure que l'emmeteur

#pragma pack(push,1)
typedef struct struct_message {
  uint8_t device_id;
  int32_t tensionCC;
  int32_t courant;
  uint8_t checksum;
} struct_message;

#pragma pack(pop)

// Create a struct_message called myData
struct_message myData;

uint8_t calculateChecksum(struct_message *data) {
  return (uint8_t)((data->device_id ^ data->tensionCC ^ data->courant) & 0xFF);
}
 



// callback function that will be executed when data is received
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(myData)) {
  Serial.printf("Taille invalide: %d\n", len);
  return;
}
  memcpy(&myData, incomingData, sizeof(myData));
  //verifie le checksum
 uint8_t expectedChecksum =calculateChecksum(&myData);
 if (myData.checksum !=expectedChecksum){
  Serial.printf("mydata.checksum:%dexpectedChecksum:%d\n",myData.checksum,expectedChecksum);
 }

  // 3) Affichage debug (source MAC + device_id)
  Serial.printf("ESP-NOW recu [%02x:%02x:%02x:%02x:%02x:%02x] device_id=%d | "
                "tension=%d mV | courant=%d mA\n",
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5],
                myData.device_id, myData.tensionCC, myData.courant);

 // Serial.printf("Espnow recu: device_id =%d |tensionCC=%d |courant=%d | checksum=%d \n",myData.device_id,myData.tensionCC,myData.courant,myData.checksum);
  //transmission par Uar vers Esp wroom
  uint8_t start_byte =0xAA;
  uint8_t stop_byte =0x55;
  SerialPort.write((uint8_t*)&start_byte,sizeof(start_byte)); 
  SerialPort.write((uint8_t*)&myData,sizeof(myData));
  SerialPort.write((uint8_t*)&stop_byte,sizeof(stop_byte));

} 
  
 


 
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("\n[ESP-c3-mini]Recepteur Espnow+ Emmeteur série");
  SerialPort.begin(115200,SERIAL_8N1,20,21); //Rx=GPIO20 tx=GPIO21
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  Serial.print("MAC: ");
  Serial.println (WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  Serial.println("ESP-NOW pret");

}
 
void loop() {

}

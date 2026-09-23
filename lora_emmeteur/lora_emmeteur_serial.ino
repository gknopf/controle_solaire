

//SP32-WROOM] Récepteur UART + Émetteur LoRa");


#include <HardwareSerial.h>
#include <SPI.h>
#include <LoRa.h>  // Librairie de Sandeep Mistry




                                                        
// --- Configuration UART (depuis ESP32-C3) ---
HardwareSerial SerialPort(2);
const int BAUD_RATE = 115200;
// RX=GPIo16, TX=GPIO17 (UART2 sur ESP32-WROOM)

// --- Configuration LoRa ---
const int LORA_CS = 5;
const int LORA_RST = 14;
const int LORA_DIO0 = 2;
const long LORA_FREQ = 868E6;  // 868 MHz (Europe) / 915E6 (Amérique)
const int LORA_TX_POWER = 20;  // Puissance d'émission (dBm)

// --- Structure des données ---
#pragma pack(push, 1)
typedef struct {
  uint8_t sensorId;
  int32_t tensionCC;
  int32_t courant;
  uint8_t checksum;
} SensorData;
#pragma pack(pop)


uint8_t calculateChecksum(SensorData *data) {
  return (uint8_t)((data->sensorId ^ data->tensionCC ^ data->courant) &0xFF);
} 

// --- Statistiques ---
unsigned long lastLoRaSend = 0;
int loraPacketCount = 0;
int uartPacketCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n[ESP32-WROOM] Récepteur UART + Émetteur LoRa");

  // --- Initialiser UART ---
  SerialPort.begin(BAUD_RATE, SERIAL_8N1, 16, 17); // RX=GPI016, TX=GPI017
  Serial.println("✅ UART initialisé");

  // --- Initialiser LoRa ---
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("❌ Échec d'initialisation LoRa !");
    while (1) delay(1000); // Bloquer si LoRa ne démarre pas
  }

  // Optimisations LoRa
  LoRa.setTxPower(LORA_TX_POWER, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(9);   // SF9 : bon compromis portée/débit
  LoRa.setSignalBandwidth(125E3); // 125 kHz
  LoRa.setCodingRate4(5);        // CR 4/5
  LoRa.enableCrc();              // CRC activé (détection d'erreurs)

  Serial.printf("✅ LoRa initialisé | Freq: %d MHz | SF: 9 | BW: 125kHz\n",
               LORA_FREQ == 868E6 ? 868 : 915);
  Serial.printf("   Puissance: %d dBm | CRC: activé\n", LORA_TX_POWER);
  Serial.println("✅ Prêt à recevoir (UART) et émettre (LoRa)\n");
}

void sendViaLoRa(SensorData *data) {
  // Ajouter un numéro de séquence pour suivre les paquets LoRa
  // (on réutilise le champ 'a' temporairement ou on ajoute un en-tête)
  loraPacketCount++;

  // Envoyer les données brutes en binaire
  LoRa.beginPacket();
  LoRa.write((uint8_t*)data, sizeof(SensorData));
  LoRa.endPacket();

  Serial.printf("📡 [LoRa] Paquet #%d envoyé (%d octets) | Capteur %d\n",
               loraPacketCount, sizeof(SensorData), data->sensorId);

  lastLoRaSend = millis();
}





void loop() {
  // --- Lire les données depuis UART ---
  if (SerialPort.available()>0){
    uint8_t start_byte=SerialPort.read();
    if (start_byte ==0xAA){
      if (SerialPort.available() >= sizeof(SensorData)) {
        SensorData receivedData;
        SerialPort.readBytes((uint8_t*)&receivedData, sizeof(receivedData));
         // Vérifier le checksum
        uint8_t expectedChecksum = calculateChecksum(&receivedData);
        if (receivedData.checksum !=expectedChecksum){
          Serial.printf("receivedata.checksum:%dexpectedChecksum:%d\n",receivedData.checksum,expectedChecksum);
        }
        uartPacketCount++;
        Serial.printf("📥 [UART] Capteur %d | tensionCC=%d | courant=%d | checksum =%d",
                   receivedData.sensorId, receivedData.tensionCC,
                   receivedData.courant, receivedData.checksum);
        // --- Transmettre par LoRa ---
        sendViaLoRa(&receivedData);
      }
     }  
  }
}  
    /*
    if (receivedData.checksum != expectedChecksum) {
      Serial.println("❌ Checksum invalide (UART) !");
      return;
    }

  // --- Statistiques toutes les 30s ---
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 30000) {
    lastStats = millis();
    Serial.printf("\n📊 Stats | UART reçus: %d | LoRa envoyés: %d\n\n",
                 uartPacketCount, loraPacketCount);
  }
*/

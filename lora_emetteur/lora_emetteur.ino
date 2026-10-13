// 20261012 ok

//ESP32-WROOM] Récepteur UART + Émetteur LoRa");


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
  int32_t tension;
  int32_t courant;
  uint8_t checksum;
} SensorData;
#pragma pack(pop)


uint8_t calculateChecksum(SensorData *data) {
  return (uint8_t)((data->sensorId ^ data->tension ^ data->courant) &0xFF);
} 

// --- Statistiques ---
unsigned long lastLoRaSend = 0;
int loraPacketCount = 0;
int uartPacketCount = 0;
int uartSyncErrors  = 0;

const uint8_t START_BYTE = 0xAA;
const uint8_t END_BYTE   = 0x55;

enum DecodeState { WAIT_START, READ_PAYLOAD, WAIT_END };
DecodeState state = WAIT_START;

uint8_t rxBuf[sizeof(SensorData)];
int     rxIndex = 0;

// Retourne true quand une trame VALIDE est prete dans *out
bool decodeUart(uint8_t b, SensorData *out) {

  switch (state) {

    case WAIT_START:
      if (b == START_BYTE) {
        rxIndex = 0;
        state = READ_PAYLOAD;
      }
      break;

    case READ_PAYLOAD:
      rxBuf[rxIndex++] = b;
      if (rxIndex >= sizeof(SensorData)) {
        state = WAIT_END;
      }
      break;

    case WAIT_END:
      state = WAIT_START;          // quoi qu'il arrive, on repart du debut
      if (b == E00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000ND_BYTE) {
        memcpy(out, rxBuf, sizeof(SensorData));
        return true;                // trame complete et cadree
      }
      // 0x55 attendu mais autre chose : trame decalee
      uartSyncErrors++;
      return false;
  }
  return false;
}

// ============================================================
//  Traitement d'une trame valide
// ============================================================
void processFrame(SensorData *data) {

  // Checksum applicatif : rejet si invalide
  uint8_t expected = calculateChecksum(data);
  if (data->checksum != expected) {
    Serial.printf("Checksum UART invalide: recu=%d attendu=%d (rejet)\n",
                  data->checksum, expected);
    return;   // rien ne part en LoRa
  }

  uartPacketCount++;
  Serial.printf("[UART] Capteur %d | tension=%d mV | courant=%d mA\n",
                data->sensorId, data->tension, data->courant);

 sendViaLoRa(data);
}

void sendViaLoRa(SensorData *data) {
  loraPacketCount++;

  LoRa.beginPacket();
  LoRa.write((uint8_t *)data, sizeof(SensorData));
  LoRa.endPacket();

  Serial.printf("[LoRa] Paquet #%d envoye (%d octets) | Capteur %d\n",
                loraPacketCount, sizeof(SensorData), data->sensorId);
  lastLoRaSend = millis();
}

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





void loop() {
  // --- Lire les données depuis UART ---

  // --- Decoder chaque octet disponible (non bloquant) ---
  while (SerialPort.available() > 0) {
    uint8_t b = SerialPort.read();
    SensorData frame;
    if (decodeUart(b, &frame)) {
      processFrame(&frame);
    }
  }

 // --- Statistiques toutes les 30 s ---
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 30000) {
    lastStats = millis();
    Serial.printf("[Stats] UART valides: %d | erreurs synchro: %d | LoRa envoyes: %d\n",
                  uartPacketCount, uartSyncErrors, loraPacketCount);
  }

}
  
  

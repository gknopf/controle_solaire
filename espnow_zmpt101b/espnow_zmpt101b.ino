#include <esp_now.h>
#include <WiFi.h>

// ============================================================
//  Capteur de tension AC ZMPT101B -> ESP32 -> ESP-NOW
//  Version corrigee :
//   - echantillonnage sur un nombre ENTIER de periodes 50 Hz
//   - calcul RMS vrai (plus de detection de crete aleatoire)
//   - offset (vzero) mesure a chaque cycle, pas de constante magique
//   - moyenne glissante conservee pour lisser la mesure
//   - ADC1 uniquement (compatible Wi-Fi actif)
// ============================================================

// ---------- Configuration materielle ----------
// IMPORTANT : utiliser une broche ADC1 (GPIO 32..39).
// Les broches ADC2 renvoient 0 quand le Wi-Fi est actif !
// Exemple : GPIO 34. Adaptez selon votre carte.
#define PIN_ZMPT   3
#define ADC_ATTN   ADC_11db          // pleine echelle ~3.3 V

// ---------- Calibration ----------
// COEF_CAL convertit le RMS "en pas d'ADC" en volts reels.
// Etalonnage : mesurer la tension secteur avec un multimetre,
// puis COEF_CAL = V_secteur / vrms_adc correspondant.
// Point de depart typique pour ZMPT101b branche au secteur :
#define COEF_CAL  0.705 // 0.905

// ---------- Echantillonnage ----------
// 50 Hz -> periode 20 ms. On echantillonne 2 periodes = 40 ms.
#define N_SAMPLES     200
#define SAMPLE_US     200            // 200 x 200 us = 40 ms

// ---------- Moyenne glissante ----------
#define N_LISSE       100            // fenetre de lissage (env. 17 min a 10 s)

// ---------- ESP-NOW ----------
uint8_t broadcastAddress[] = {0xe0, 0x72, 0xa1, 0x72, 0x34, 0xec};

#pragma pack(push, 1)
typedef struct struct_message {
  uint8_t  device_id;
  int32_t  tensionAC;   // tension RMS en mV
  int32_t  courantAC;   // courant en mA
  uint8_t  checksum;
} struct_message;
#pragma pack(pop)

struct_message myData;
esp_now_peer_info_t peerInfo;

uint8_t calculateChecksum(struct_message *data) {
  return (uint8_t)((data->device_id ^ data->tensionAC ^ data->courantAC) & 0xFF);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nStatut envoi : ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Succes" : "Echec");
}

// ============================================================
//  Mesure : RMS vrai sur 2 periodes completes
// ============================================================
float mesurerTensionRMS() {
  static int samples[N_SAMPLES];

  // --- Acquisition : pas temporel regulier, 40 ms au total ---
  noInterrupts();                     // eviter la derive du timing pendant la capture
  for (int i = 0; i < N_SAMPLES; i++) {
    samples[i] = analogRead(PIN_ZMPT);
    delayMicroseconds(SAMPLE_US);
  }
  interrupts();

  // --- Offset reel (composante continue) ---
  long somme = 0;
  for (int i = 0; i < N_SAMPLES; i++) somme += samples[i];
  float vzero = (float)somme / N_SAMPLES;

  // --- RMS vrai : sqrt(moyenne des (v - vzero)^2) ---
  float sumSq = 0;
  for (int i = 0; i < N_SAMPLES; i++) {
    float v = samples[i] - vzero;
    sumSq += v * v;
  }
  float vrms_adc = sqrtf(sumSq / N_SAMPLES);

  return vrms_adc * COEF_CAL;         // volts RMS
}

// ============================================================
//  Lissage : moyenne glissante identique a l'original
// ============================================================
float lisser(float nouvelleValeur) {
  static float fenetre[N_LISSE];
  static int   index = 0;
  static long  count = 0;

  fenetre[index] = nouvelleValeur;
  index = (index + 1) % N_LISSE;
  if (count < N_LISSE) count++;

  float somme = 0;
  for (int i = 0; i < count; i++) somme += fenetre[i];
  return somme / count;
}

void setup() {
  Serial.begin(115200);

  // --- ADC ---
  analogSetAttenuation(ADC_ATTN);
  analogReadResolution(12);
  // analogSetPinAttenuation(PIN_ZMPT, ADC_ATTN);  // si attenuation par broche souhaitee

  // --- Wi-Fi + ESP-NOW ---
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur initialisation ESP-NOW");
    return;
  }
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Echec ajout du pair");
    return;
  }

  myData.device_id = 2;   // ID du capteur de tension AC
}

void loop() {
  // 1) Mesure RMS
  float tensionAC = mesurerTensionRMS();

  // 2) Lissage
  float tensionLissee = lisser(tensionAC);
  Serial.print("Tension AC : ");
  Serial.print(tensionLissee, 1);
  Serial.println(" V");

  // 3) Envoi ESP-NOW (mV)
  myData.tensionAC  = static_cast<int32_t>(tensionLissee * 1000.0f);
  myData.courantAC  = 0;
  myData.checksum   = calculateChecksum(&myData);

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  if (result == ESP_OK) {
    Serial.println("Donnees envoyees avec succes");
  } else {
    Serial.println("Echec envoi");
  }

 // delay(10000);   // periode d'envoi
}

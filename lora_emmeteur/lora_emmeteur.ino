#include <LoRa.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

// Définition des broches pour ESP32
#define SS      5
#define RST     14
#define DIO0    2

// Structure de données à envoyer
struct DataPacket {
  int a;
  float b;
  char c;
};

// Création de la structure
DataPacket packet;
DataPacket myData;

//---------------------eespnow
// Adresse MAC de l'émetteur (à remplacer par celle de votre émetteur)
// Vous pouvez utiliser BROADCAST pour recevoir de tous les émetteurs
uint8_t broadcastAddress[] = {0xe0, 0x72, 0xa1, 0x72, 0x33, 0xbc};

// Variables pour les statistiques
unsigned long lastReceiveTime = 0;
int packetCount = 0;

// Fonction callback lors de la réception de données

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
  // Copie des données reçues dans la structure
  memcpy(&myData, incomingData, sizeof(myData));
  
  // Mise à jour du temps de dernière réception
  lastReceiveTime = millis();
  packetCount++;
  
  // Affichage des informations
  Serial.println("\n=== PAQUET REÇU ===");
  Serial.print("Taille des données : ");
  Serial.print(len);
  Serial.println(" octets");
  
  // Affichage des données
  Serial.print("a (int) : ");
  Serial.println(myData.a);
  
  Serial.print("b (float) : ");
  Serial.println(myData.b, 4);  // Affichage avec 4 décimales
  
  Serial.print("c (char) : ");
  Serial.println(myData.c);
  
 
  
  Serial.println("==================\n");
}

//-------------------------lora

// Fréquence LoRa (868 MHz en Europe, 915 MHz en Amérique)
#define FREQUENCY 868E6

// Mode : true = émetteur, false = récepteur
bool isTransmitter = true;  // Changez cette valeur pour basculer

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("ESP32 LoRa Test");
  
  // Initialisation LoRa
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("Erreur d'initialisation LoRa !");
    while (1);
  }
  
  // Configuration des paramètres LoRa
  LoRa.setSpreadingFactor(12);      // SF12 pour une meilleure portée
  LoRa.setSignalBandwidth(125E3);   // Bande passante 125 kHz
  LoRa.setCodingRate4(5);           // Taux de codage 4/5
  LoRa.setTxPower(20);              // Puissance d'émission 20 dBm
  
  Serial.println("LoRa initialisé !");
  
  if (isTransmitter) {
    Serial.println("Mode : ÉMETTEUR");
    // Initialisation des données
    packet.a = 42;
    packet.b = 3.14159;
    packet.c = 'X';
  } else {
    Serial.println("Mode : RÉCEPTEUR");
  }

//------------------espnow
  Serial.println("\n=== RÉCEPTEUR ESP-NOW ===");
  Serial.println("Initialisation...");
  
  // Configuration du mode WiFi
  WiFi.mode(WIFI_STA);
  
  // Initialisation ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur lors de l'initialisation d'ESP-NOW");
    return;
  }
  
  // Enregistrement de la fonction callback de réception
  esp_now_register_recv_cb(OnDataRecv);
  
  // Ajout de l'émetteur (ou broadcast pour tous)
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Erreur lors de l'ajout du peer");
    return;
  }
  
  // Affichage de l'adresse MAC du récepteur
  Serial.print("Adresse MAC du récepteur : ");
  Serial.println(WiFi.macAddress());
  
  Serial.println("Récepteur ESP-NOW prêt !");
  Serial.println("En attente de données...\n");




  
}

void loop() {
  // ------------------------------esp_now
  static unsigned long lastStatsTime = 0;
  
  if (millis() - lastStatsTime > 10000) {
    lastStatsTime = millis();
    
    // Vérification si des données ont été reçues récemment
    if (millis() - lastReceiveTime < 10000) {
      Serial.print("Statistiques : ");
      Serial.print(packetCount);
      Serial.println(" paquets reçus au total");
      Serial.println("Réception active");
    } else {
      Serial.println("Aucune donnée reçue depuis 10s");
    }
    
  }


  //--------------                  lora
  if (isTransmitter) {
    // Fonction d'émission
    sendPacket();
    delay(5000);  // Envoi toutes les 5 secondes
  } else {
    // Fonction de réception
    receivePacket();
  }
}

// Fonction pour envoyer le paquet
void sendPacket() {
  Serial.println("\n--- Envoi du paquet ---");
  
  // Modification des données à chaque envoi
  packet.a++;
  packet.b += 0.1;
  packet.c = (packet.c == 'Z') ? 'A' : packet.c + 1;
  
  // Démarrage du paquet
  LoRa.beginPacket();
  
  // Envoi des données de la structure
  LoRa.write((uint8_t*)&packet, sizeof(DataPacket));
  
  // Fin du paquet
  LoRa.endPacket();
  
  // Affichage des données envoyées
  Serial.print("a = ");
  Serial.println(packet.a);
  Serial.print("b = ");
  Serial.println(packet.b, 4);  // 4 décimales
  Serial.print("c = ");
  Serial.println(packet.c);
  
  Serial.println("Paquet envoyé !");
}

// Fonction pour recevoir le paquet
void receivePacket() {
  // Vérification si un paquet est disponible
  int packetSize = LoRa.parsePacket();
  
  if (packetSize > 0) {
    Serial.println("\n--- Paquet reçu ---");
    
    // Réception des données dans la structure
    if (packetSize == sizeof(DataPacket)) {
      LoRa.readBytes((uint8_t*)&packet, sizeof(DataPacket));
      
 // Affichage des données reçues
      Serial.print("Taille du paquet : ");
      Serial.println(packetSize);
      Serial.print("a:");
      Serial.println(packet.a);


    }
  }
}

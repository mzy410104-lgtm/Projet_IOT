
#include "WiFi.h"
#include <HardwareSerial.h>

// --- Configuration LoRa E5 ---
HardwareSerial loraSerial(2); // UART2: RX=16, TX=17

// --- TTN (OTAA)  ---
const char* devEUI = "70B3D57ED0073415";
const char* appEUI = "0000000000000000";
const char* appKey = "44B8F00FFE627DC84F204831CF7519C6";


// Structure : stocke les informations relatives aux points d'accès détectés.
struct APData {
  String mac;
  int8_t rssi;
};
APData topAPs[3] = {
  {"00:00:00:00:00:00", -127},
  {"00:00:00:00:00:00", -127},
  {"00:00:00:00:00:00", -127}
};

unsigned long previousMillis = 0;
const long interval = 60000; // Envoyer toutes les 60 secondes

// --- Déclaration de fonction ---
void macToBytes(String macStr, byte* macBytes);
String sendATCommand(String command, int timeout);
void initializeLoRa();
bool isRandomMAC(String mac); // Fonction de filtrage principale

void setup() {
  Serial.begin(115200);
  loraSerial.begin(9600, SERIAL_8N1, 16, 17);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Démarrage du système : mode de filtrage universel (filtre uniquement les adresses MAC aléatoires)...");
  initializeLoRa();
}

void loop() {
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    
    //1. Analyser et filtrer
    scanAndSelectTop3();
    
    // 2. Transmettre les données (si au moins une adresse MAC non aléatoire est trouvée)
    if (topAPs[0].rssi > -127) {
      sendLoRaData3();
    } else {
      Serial.println("Aucun routeur fixe n'a été détecté lors de cette analyse ; transmission ignorée.");
    }
  }
}

// --- Logique principale : analyse et tri ---
void scanAndSelectTop3() {
  Serial.println("开始 WiFi 扫描...");
  int n = WiFi.scanNetworks();
  
  //Réinitialiser les trois premiers
  for(int i=0; i<3; i++) { 
    topAPs[i].mac = "00:00:00:00:00:00"; 
    topAPs[i].rssi = -127; 
  }

  if (n == 0) return;

  Serial.printf("%d réseaux,Filtrage...\n", n);

  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    bssid.toUpperCase(); 

    if (!isRandomMAC(bssid)) {
      int32_t rssi = WiFi.RSSI(i);
      String ssid = WiFi.SSID(i);
      
      // Sortie de débogage : voyons qui a été sélectionné
      Serial.printf("  [Accept] %s (%s) %d dBm\n", ssid.c_str(), bssid.c_str(), rssi);

      if (rssi > topAPs[0].rssi) {
        topAPs[2] = topAPs[1]; topAPs[1] = topAPs[0];
        topAPs[0] = {bssid, (int8_t)rssi};
      } else if (rssi > topAPs[1].rssi) {
        topAPs[2] = topAPs[1];
        topAPs[1] = {bssid, (int8_t)rssi};
      } else if (rssi > topAPs[2].rssi) {
        topAPs[2] = {bssid, (int8_t)rssi};
      }
    } else {

    }
  }
}

// --- Logique de transmission (21 octets) ---
void sendLoRaData3() {
  byte payload[21];
  for(int i=0; i<3; i++) {
    macToBytes(topAPs[i].mac, &payload[i*7]);
    payload[i*7 + 6] = topAPs[i].rssi;
  }
  
  String hexPayload = "";
  for(int i = 0; i < 21; i++) {
    char hex[3];
    sprintf(hex, "%02X", payload[i]);
    hexPayload += hex;
  }
  
  Serial.println("LoRa envoye (HEX): " + hexPayload);
  loraSerial.println("AT+MSGHEX=" + hexPayload); 
}

// --- Fonction auxiliaire principale : détecter une adresse MAC aléatoire ---
bool isRandomMAC(String mac) {
  // Format d'adresse MAC « XX:XX:... »
  if (mac.length() < 2) return false;

  char c = mac.charAt(1); 
  
  // Vérifier s'il s'agit de 2, 6, A, E (sans distinction entre majuscules et minuscules)
  if (c == '2' || c == '6' || 
      c == 'A' || c == 'a' || 
      c == 'E' || c == 'e') {
    return true; // Est une adresse MAC aléatoire (adresse administrative locale → point d'accès mobile)
  }
  return false; // Il s'agit d'une adresse MAC universelle (adresse unique au monde → routeur fixe).
}

void macToBytes(String macStr, byte* macBytes) {
  sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
         &macBytes[0], &macBytes[1], &macBytes[2], 
         &macBytes[3], &macBytes[4], &macBytes[5]);
}

String sendATCommand(String command, int timeout) {
  Serial.println("CMD: " + command);
  loraSerial.println(command);
  delay(100);
  String resp = "";
  long start = millis();
  while (millis() - start < timeout) {
    while(loraSerial.available()) resp += (char)loraSerial.read();
  }
  resp.trim();
  if(resp.length() > 0) Serial.println("RESP: " + resp);
  return resp;
}

void initializeLoRa() {
  sendATCommand("AT", 1000);
  sendATCommand("AT+MODE=LWOTAA", 1000);
  sendATCommand("AT+DR=EU868", 1000);
  sendATCommand("AT+ID=DevEui," + String(devEUI), 1000);
  sendATCommand("AT+ID=AppEui," + String(appEUI), 1000);
  sendATCommand("AT+KEY=APPKEY," + String(appKey), 1000);
  sendATCommand("AT+JOIN", 5000);
  delay(5000);
}

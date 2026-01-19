

#include "WiFi.h"

// Si les valeurs sont 2, 6, A, E, cela indique que l'adresse est un point d'accès.
bool isRandomMAC(String mac) {
  if (mac.length() < 2) return false; // Contrôle de sécurité

  char c = mac.charAt(1); // Récupérer le deuxième caractère (index 1)
  
  // Vérifiez s'il s'agit de 2, 6, A, E (sans distinction entre majuscules et minuscules)
  if (c == '2' || c == '6' || 
      c == 'A' || c == 'a' || 
      c == 'E' || c == 'e') {
    return true; // Est une adresse MAC aléatoire (potentiellement un point d'accès)
  }
  return false; // Il s'agit d'un MAC universel (probablement un routeur fixe).
}

void setup() {
  Serial.begin(115200); 
  
// Réglez le WiFi en mode client (STA) et déconnectez-vous
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("WiFi La détection a commencé.");
  Serial.println("Filtrer les adresses MAC aléatoires dont le deuxième chiffre est 2/6/A/E....");
  Serial.println("Format de sortie :SSID,BSSID,RSSI,Channel");
  Serial.println("-------------------------------------------------");
}

void loop() {
  Serial.println("\nCommencer la numérisation...");
  
  int n = WiFi.scanNetworks();
  
  Serial.println("Scan terminé.");

  if (n == 0) {
    Serial.println("Aucun réseau trouvé.");
  } else {
    int fixed_count = 0; 

    Serial.println("--- Il peut s'agir d'un réseau à routeur fixe. ---");
    Serial.println("SSID,BSSID,RSSI,Channel");

    for (int i = 0; i < n; ++i) {
      String bssid = WiFi.BSSIDstr(i); // Obtenir l'adresse MAC
      
      if (!isRandomMAC(bssid)) {
        
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        int32_t channel = WiFi.channel(i);

        Serial.print(ssid);
        Serial.print(",");
        Serial.print(bssid);
        Serial.print(",");
        Serial.print(rssi);
        Serial.print(",");
        Serial.println(channel);
        
        fixed_count++;
      }
    }
    
    Serial.printf("--- Statistiques: Découverte %d , Conserver %d 个 ---\n", n, fixed_count);
  }

  // Veuillez patienter 10 secondes.
  delay(10000); 
}
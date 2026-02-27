#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  SERVEUR TERRAIN - Heltec WiFi LoRa 32 V4 (ESP32-S3)
//  Brigade des Sangliers - Airsoft Game System
//
//  Role : passerelle LoRa <-> MQTT
//    1. Recoit les paquets JSON de la valise via LoRa 868 MHz
//    2. Se connecte au hotspot du smartphone via WiFi
//    3. Publie les evenements sur le broker MQTT du serveur applicatif
// ============================================================

// --- OLED integre Heltec V4 (GPIO fixes hardware) ---
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21
#define OLED_ADDR   0x3C

// --- LoRa SX1262 (GPIO fixes hardware Heltec V4) ---
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
// Doit etre identique a la valise
#define LORA_FREQ   868.0
#define LORA_BW     125.0
#define LORA_SF     9
#define LORA_CR     5
#define LORA_POWER  14
#define LORA_PREAMBLE 8

// --- WiFi - Hotspot du smartphone ---
#define WIFI_SSID   "BrigadeSangliers"
#define WIFI_PASS   "airsoft2024"
#define WIFI_TIMEOUT_MS 20000

// --- MQTT Broker ---
// IP du serveur applicatif (Proxmox/Docker)
// Peut etre l'IP du hotspot si le backend tourne sur le tel,
// ou l'IP publique/VPN de votre serveur Proxmox
#define MQTT_BROKER     "192.168.43.100"    // A adapter
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "brigade-field-server"
#define MQTT_USER       ""                  // Laisser vide si pas d'auth
#define MQTT_PASS       ""

// Topics MQTT
#define MQTT_TOPIC_EVENTS   "brigade/game/event"    // Publish (valise -> backend)
#define MQTT_TOPIC_COMMANDS "brigade/game/command"  // Subscribe (backend -> valise, optionnel)

// --- Reconnexion ---
#define MQTT_RECONNECT_MS   5000    // Intervalle tentative reconnexion MQTT
#define LORA_RX_TIMEOUT_MS  500     // Timeout reception LoRa (mode polling)

#endif

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  VALISE - Heltec WiFi LoRa 32 V4 (ESP32-S3) Configuration
//  Brigade des Sangliers - Airsoft Game System
//
//  Communication :
//    - ESP-NOW (2.4 GHz WiFi) <-> Satellites ESP32-C3
//    - LoRa SX1262 (868 MHz)  <-> Serveur terrain (Heltec V4 #2)
// ============================================================

// --- Ruban LED WS2812B ---
#define LED_PIN         38
#define LED_COUNT       16
#define LED_BRIGHTNESS  180

// --- Boutons (actif LOW) ---
// Bouton A = Valider / Demarrer partie
// Bouton B = Annuler / Fin de partie
#define BTN_A_PIN   0   // GPIO0 = BOOT sur Heltec V4
#define BTN_B_PIN   3   // GPIO3 = PRG  sur Heltec V4
#define DEBOUNCE_MS 80

// --- OLED integre Heltec V4 (SSD1306 128x64 - GPIO fixes hardware) ---
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21
#define OLED_ADDR   0x3C

// --- Clavier matriciel 4x4 ---
#define KBD_ROW0  11
#define KBD_ROW1  12
#define KBD_ROW2  13
#define KBD_ROW3  14
#define KBD_COL0  15
#define KBD_COL1  16
#define KBD_COL2  35
#define KBD_COL3  36

// --- GPS UART (optionnel) ---
#define GPS_RX_PIN  44
#define GPS_TX_PIN  43
#define GPS_BAUD    9600

// --- LoRa SX1262 (GPIO fixes hardware Heltec V4) ---
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
// Parametres radio - Europe 868 MHz (bande ISM)
#define LORA_FREQ   868.0   // MHz
#define LORA_BW     125.0   // kHz
#define LORA_SF     9       // Spreading Factor (7-12, plus grand = plus loin/lent)
#define LORA_CR     5       // Coding Rate 4/5
#define LORA_POWER  14      // dBm (max 22 dBm legalement 14 dBm en 868 MHz)
#define LORA_PREAMBLE 8

// --- ESP-NOW (pour communication avec les satellites) ---
#define ESPNOW_CHANNEL      1
#define MAX_SATELLITES      20
#define PROXIMITY_RSSI      -55     // Seuil RSSI ~1m (calibrer sur terrain)
#define SATELLITE_TIMEOUT_MS 5000
#define PING_INTERVAL_MS    1000

// --- Couleurs ---
#define COLOR_NONE    0x000000
#define COLOR_RED     0xFF2000
#define COLOR_BLUE    0x0044FF
#define COLOR_GREEN   0x00FF00
#define COLOR_YELLOW  0xFFCC00

#define TEAM_COUNT  5
const char* TEAM_NAMES[TEAM_COUNT]     = {"---", "Rouge", "Bleu", "Vert", "Jaune"};
const uint32_t TEAM_COLORS[TEAM_COUNT] = {
    COLOR_NONE, COLOR_RED, COLOR_BLUE, COLOR_GREEN, COLOR_YELLOW
};

#define ROLE_COUNT  5
const char* ROLE_NAMES[ROLE_COUNT] = {"Soldat", "Medic", "Cmd.", "Sniper", "Scout"};

#endif

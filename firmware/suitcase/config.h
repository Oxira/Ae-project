#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  VALISE - Heltec WiFi LoRa 32 V4 (ESP32-S3) Configuration
//  Brigade des Sangliers - Airsoft Game System
//  Adaptez selon votre cablage
// ============================================================

// --- Ruban LED WS2812B (couleur d'equipe + animations) ---
#define LED_PIN         38
#define LED_COUNT       16
#define LED_BRIGHTNESS  180

// --- Boutons RGB (actif LOW) ---
// Bouton A = Valider / Suivant / Demarrer partie
// Bouton B = Annuler / Retour / Fin de partie
#define BTN_A_PIN   0   // GPIO0 = BOOT button sur Heltec V4
#define BTN_B_PIN   3   // GPIO3 = PRG button sur Heltec V4
#define DEBOUNCE_MS 80

// --- OLED integre Heltec V4 (SSD1306 128x64) ---
// Ces GPIO sont fixes par le hardware Heltec V4
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21
#define OLED_ADDR   0x3C

// --- Clavier matriciel 4x4 ---
// Adaptez les GPIO selon votre cablage
#define KBD_ROW0  11
#define KBD_ROW1  12
#define KBD_ROW2  13
#define KBD_ROW3  14
#define KBD_COL0  15
#define KBD_COL1  16
#define KBD_COL2  35
#define KBD_COL3  36

// --- GPS UART ---
#define GPS_RX_PIN  44
#define GPS_TX_PIN  43
#define GPS_BAUD    9600

// --- WiFi (point d'acces du smartphone / Raspberry Pi) ---
#define WIFI_SSID   "BrigadeSangliers"
#define WIFI_PASS   "airsoft2024"
// IP du serveur (hotspot Android = souvent 192.168.43.1)
#define SERVER_URL  "http://192.168.43.1:3000"

// --- ESP-NOW ---
#define ESPNOW_CHANNEL      1       // Canal WiFi utilise pour ESP-NOW
#define MAX_SATELLITES      20      // Nb max de satellites geres
// Seuil RSSI pour detection proximite ~1m (ajustable selon materiel)
// -50 dBm = tres proche, -70 dBm = loin ; a calibrer sur le terrain
#define PROXIMITY_RSSI      -55

// --- Timeouts ---
#define WIFI_TIMEOUT_MS         15000
#define HTTP_TIMEOUT_MS         3000
#define SATELLITE_TIMEOUT_MS    5000    // Satellite considere absent si pas de heartbeat
#define PING_INTERVAL_MS        1000    // Intervalle de PING broadcast

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

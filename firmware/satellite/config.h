#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  SATELLITE - ESP32-C3 Pin Configuration
//  Brigade des Sangliers - Airsoft Game System
//  Adaptez ces valeurs a votre cablage specifique
// ============================================================

// --- RGB LED Strip (WS2812B) ---
#define LED_PIN         10      // GPIO data pin pour le ruban WS2812B
#define LED_COUNT       8       // Nombre de LEDs dans le ruban
#define LED_BRIGHTNESS  150     // Luminosite max (0-255)

// --- Bouton ---
#define BUTTON_PIN      9       // GPIO bouton (actif LOW, pull-up interne)
#define DEBOUNCE_MS     50      // Temps anti-rebond en millisecondes

// --- Ecran OLED SSD1306 128x64 via I2C ---
#define OLED_SDA        8
#define OLED_SCL        7
#define OLED_ADDR       0x3C
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// --- Communication ESP-NOW ---
#define ESPNOW_CHANNEL  1       // Doit correspondre au canal WiFi de la valise
#define HEARTBEAT_MS    500     // Intervalle heartbeat en ms
#define PAIR_RETRY_MS   2000    // Intervalle de tentative de pairing

// --- Animation LED ---
#define BLINK_ON_MS     300     // Duree allume lors du clignotement (mort)
#define BLINK_OFF_MS    300     // Duree eteint lors du clignotement

// --- Couleurs des equipes (RGB) ---
#define COLOR_NONE       0x000000
#define COLOR_RED        0xFF2000
#define COLOR_BLUE       0x0044FF
#define COLOR_GREEN      0x00FF00
#define COLOR_YELLOW     0xFFCC00
#define COLOR_DEAD_BLINK 0xFF0000   // Rouge clignotant = elimine

// --- Noms des roles ---
#define ROLE_COUNT  5
const char* ROLE_NAMES[ROLE_COUNT] = {"Soldat", "Medic", "Cmd.", "Sniper", "Scout"};

// --- Noms et couleurs des equipes ---
#define TEAM_COUNT  5
const char* TEAM_NAMES[TEAM_COUNT]    = {"---", "Rouge", "Bleu", "Vert", "Jaune"};
const uint32_t TEAM_COLORS[TEAM_COUNT] = {
    COLOR_NONE, COLOR_RED, COLOR_BLUE, COLOR_GREEN, COLOR_YELLOW
};

#endif

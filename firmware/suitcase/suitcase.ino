/*
 * ============================================================
 *  Brigade des Sangliers - Firmware Valise
 *  Materiel : Heltec WiFi LoRa 32 V4 (ESP32-S3)
 *
 *  La valise est le hub central du systeme :
 *  - Decouvre les satellites par ESP-NOW (2.4 GHz)
 *  - Permet la saisie des noms, equipes et roles (clavier 4x4)
 *  - Gere la partie : demarrage, morts, revives, fin
 *  - Detecte la proximite des satellites via RSSI (~1m)
 *  - Transmet les evenements au SERVEUR TERRAIN via LoRa 868 MHz
 *    (Le serveur terrain les relaie ensuite par MQTT vers le backend Java)
 *
 *  Librairies requises (Arduino Library Manager) :
 *    - Adafruit NeoPixel
 *    - Adafruit SSD1306 + Adafruit GFX
 *    - ArduinoJson
 *    - RadioLib (pour LoRa SX1262)
 *
 *  Librairie Heltec : https://github.com/HelTecAutomation/Heltec_ESP32
 * ============================================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <RadioLib.h>
#include "config.h"

// ============================================================
// LORA SX1262 (Heltec V4 - GPIO fixes hardware)
// ============================================================
SX1262 lora = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
bool loraOk = false;

// ============================================================
// PROTOCOLE (identique satellite.ino / suitcase.ino)
// ============================================================
#define PKT_PING        0x01
#define PKT_ACK         0x02
#define PKT_ASSIGN      0x03
#define PKT_GAME_START  0x04
#define PKT_GAME_END    0x05
#define PKT_BUTTON      0x06
#define PKT_REVIVE      0x07
#define PKT_HEARTBEAT   0x08
#define PKT_RESET       0x09

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  sat_id;
    uint8_t  team;
    uint8_t  role;
    uint8_t  status;
    uint32_t deaths;
    char     player[24];
} GamePacket;

// ============================================================
// STRUCTURE SATELLITE
// ============================================================
struct SatelliteInfo {
    bool     registered;
    uint8_t  mac[6];
    char     player[24];
    uint8_t  team;
    uint8_t  role;
    uint8_t  status;       // 0=ok, 1=out
    uint32_t deaths;
    uint32_t lastSeen;     // millis() du dernier heartbeat
    int8_t   lastRssi;     // RSSI du dernier heartbeat
    bool     assigned;
};

// ============================================================
// ETAT DE LA MACHINE
// ============================================================
enum GameState {
    STATE_DISCOVERY,   // Decouverte des satellites
    STATE_CONFIG,      // Saisie des joueurs
    STATE_GAME,        // Partie en cours
    STATE_RESULTS      // Fin de partie - affichage resultats
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================
SatelliteInfo satellites[MAX_SATELLITES];
uint8_t       satCount     = 0;
GameState     gState       = STATE_DISCOVERY;
bool          wifiOk       = false;

// Navigation UI
int           menuSat      = 0;   // Satellite selectionne dans le menu
int           menuPage     = 0;   // Page d'affichage des resultats
char          inputBuf[24] = {0};
int           inputLen     = 0;
int           inputField   = 0;   // 0=nom, 1=equipe, 2=role
int           teamSel      = 1;
int           roleSel      = 0;

// Timers
uint32_t lastPing       = 0;
uint32_t lastDisplay    = 0;
uint32_t lastBtnA       = 0;
uint32_t lastBtnB       = 0;

// ============================================================
// MATERIEL
// ============================================================
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306  oled(128, 64, &Wire, OLED_RST);

// ============================================================
// CLAVIER 4x4
// ============================================================
const uint8_t KB_ROWS[4] = {KBD_ROW0, KBD_ROW1, KBD_ROW2, KBD_ROW3};
const uint8_t KB_COLS[4] = {KBD_COL0, KBD_COL1, KBD_COL2, KBD_COL3};

// Layout du clavier (ligne par ligne, 4x4)
// 1 2 3 A
// 4 5 6 B
// 7 8 9 C
// * 0 # D
const char KB_MAP[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

// Mapping T9 pour la saisie de noms
const char* T9_MAP[10] = {
    " 0",      // 0
    "1.,!?",   // 1
    "ABC2",    // 2
    "DEF3",    // 3
    "GHI4",    // 4
    "JKL5",    // 5
    "MNO6",    // 6
    "PQRS7",   // 7
    "TUV8",    // 8
    "WXYZ9"    // 9
};

int     lastKbdKey     = -1;
int     t9Idx          = 0;
uint32_t lastKbdTime   = 0;
const uint32_t T9_TIMEOUT = 1200;

void kbdSetup() {
    for (int r = 0; r < 4; r++) {
        pinMode(KB_ROWS[r], OUTPUT);
        digitalWrite(KB_ROWS[r], HIGH);
    }
    for (int c = 0; c < 4; c++) {
        pinMode(KB_COLS[c], INPUT_PULLUP);
    }
}

// Retourne le caractere de la touche pressée, ou 0 si rien
char kbdScan() {
    for (int r = 0; r < 4; r++) {
        digitalWrite(KB_ROWS[r], LOW);
        for (int c = 0; c < 4; c++) {
            if (digitalRead(KB_COLS[c]) == LOW) {
                delay(15); // debounce
                if (digitalRead(KB_COLS[c]) == LOW) {
                    while (digitalRead(KB_COLS[c]) == LOW) delay(5);
                    digitalWrite(KB_ROWS[r], HIGH);
                    return KB_MAP[r][c];
                }
            }
        }
        digitalWrite(KB_ROWS[r], HIGH);
    }
    return 0;
}

// Gestion saisie T9 pour les noms
void handleNameInput(char key) {
    uint32_t now = millis();

    if (key >= '0' && key <= '9') {
        int digit  = key - '0';
        int numCh  = strlen(T9_MAP[digit]);

        if (digit == lastKbdKey && now - lastKbdTime < T9_TIMEOUT) {
            // Meme touche, cycle suivant
            if (inputLen > 0) { inputLen--; inputBuf[inputLen] = '\0'; }
            t9Idx = (t9Idx + 1) % numCh;
        } else {
            t9Idx = 0;
        }
        if (inputLen < 23) {
            inputBuf[inputLen++] = T9_MAP[digit][t9Idx];
            inputBuf[inputLen]   = '\0';
        }
        lastKbdKey  = digit;
        lastKbdTime = now;
    } else if (key == '*') {
        // Supprimer dernier caractere
        if (inputLen > 0) { inputLen--; inputBuf[inputLen] = '\0'; }
        lastKbdKey = -1;
    } else if (key == '#') {
        // Espace
        if (inputLen < 23) { inputBuf[inputLen++] = ' '; inputBuf[inputLen] = '\0'; }
        lastKbdKey = -1;
    }
}

// ============================================================
// HELPERS AFFICHAGE
// ============================================================
void oledClear() {
    oled.clearDisplay();
}

void oledLine(int x, int y, int size, const char* txt) {
    oled.setTextSize(size);
    oled.setCursor(x, y);
    oled.print(txt);
}

void oledShow() { oled.display(); }

void showDiscovery() {
    oledClear();
    oled.setTextColor(SSD1306_WHITE);
    oledLine(0, 0,  1, "DECOUVERTE SATELLITES");
    char buf[32];
    snprintf(buf, sizeof(buf), "Trouves: %d", satCount);
    oledLine(0, 16, 2, buf);
    oledLine(0, 40, 1, "[A] = Config joueurs");
    oledLine(0, 52, 1, "[B] = Reset tout");
    oledShow();
}

void showConfigMenu() {
    oledClear();
    oled.setTextColor(SSD1306_WHITE);

    char title[32];
    snprintf(title, sizeof(title), "SAT %d/%d : Config", menuSat + 1, satCount);
    oledLine(0, 0, 1, title);

    if (satCount == 0) {
        oledLine(0, 20, 1, "Aucun satellite");
        oledLine(0, 32, 1, "detecte");
    } else {
        SatelliteInfo& s = satellites[menuSat];
        // Affiche le champ en cours de saisie
        switch (inputField) {
            case 0:
                oledLine(0, 14, 1, "Nom du joueur:");
                oledLine(0, 26, 2, inputBuf);
                oledLine(0, 52, 1, "[A]=OK [*]=Effacer");
                break;
            case 1:
                {
                    char line[32];
                    snprintf(line, sizeof(line), "Equipe: %s",
                             teamSel < TEAM_COUNT ? TEAM_NAMES[teamSel] : "?");
                    oledLine(0, 14, 1, line);
                    oledLine(0, 26, 2, teamSel < TEAM_COUNT ? TEAM_NAMES[teamSel] : "?");
                    oledLine(0, 52, 1, "[A]=OK [C]=Changer");
                }
                break;
            case 2:
                {
                    char line[32];
                    snprintf(line, sizeof(line), "Role: %s",
                             roleSel < ROLE_COUNT ? ROLE_NAMES[roleSel] : "?");
                    oledLine(0, 14, 1, line);
                    oledLine(0, 26, 2, roleSel < ROLE_COUNT ? ROLE_NAMES[roleSel] : "?");
                    oledLine(0, 52, 1, "[A]=OK [D]=Changer");
                }
                break;
        }
    }
    oledShow();
}

void showGameScreen() {
    oledClear();
    oled.setTextColor(SSD1306_WHITE);
    oledLine(0, 0, 1, "=== PARTIE EN COURS ===");

    int alive = 0, dead = 0;
    for (int i = 0; i < satCount; i++) {
        if (satellites[i].registered) {
            if (satellites[i].status == 0) alive++; else dead++;
        }
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "Vivants: %d  OUT: %d", alive, dead);
    oledLine(0, 14, 1, buf);

    // Affiche les 3 premiers satellites
    int y = 26;
    for (int i = 0; i < satCount && i < 3; i++) {
        SatelliteInfo& s = satellites[i];
        const char* st = s.status == 0 ? "OK" : "OUT";
        snprintf(buf, sizeof(buf), "%-10.10s %s M:%lu",
                 s.player, st, (unsigned long)s.deaths);
        oledLine(0, y, 1, buf);
        y += 12;
    }
    oledLine(0, 56, 1, "[B]=Fin de partie");
    oledShow();
}

void showResults() {
    oledClear();
    oled.setTextColor(SSD1306_WHITE);
    oledLine(0, 0, 1, "=== RESULTATS ===");
    int y = 14;
    for (int i = 0; i < satCount && i < 4; i++) {
        SatelliteInfo& s = satellites[i];
        char buf[32];
        snprintf(buf, sizeof(buf), "%-10.10s M:%lu",
                 s.player, (unsigned long)s.deaths);
        oledLine(0, y, 1, buf);
        y += 12;
    }
    oledLine(0, 56, 1, "[A]=Nouvelle partie");
    oledShow();
}

// ============================================================
// HELPERS LED
// ============================================================
void setLEDs(uint32_t color) {
    strip.fill(color);
    strip.show();
}

// ============================================================
// LORA - Envoi evenement au serveur terrain
// ============================================================

// Paquet LoRa : JSON compact (<= 250 octets, limite LoRa)
// Le serveur terrain le parse et le relaie via MQTT
void sendLoRaEvent(const char* type, const SatelliteInfo* sat) {
    if (!loraOk) return;

    StaticJsonDocument<200> doc;
    doc["type"] = type;
    doc["ts"]   = millis();
    if (sat) {
        doc["sat"]    = (int)(sat - satellites);
        doc["player"] = sat->player;
        doc["team"]   = sat->team < TEAM_COUNT ? TEAM_NAMES[sat->team] : "?";
        doc["role"]   = sat->role  < ROLE_COUNT ? ROLE_NAMES[sat->role]  : "?";
        doc["deaths"] = sat->deaths;
        doc["status"] = sat->status == 0 ? "alive" : "out";
    }

    char buf[200];
    size_t n = serializeJson(doc, buf, sizeof(buf));

    int state = lora.transmit((uint8_t*)buf, n);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa TX] %s (%d octets)\n", type, (int)n);
    } else {
        Serial.printf("[LoRa TX] Erreur %d\n", state);
    }
}

void sendLoRaGameEvent(const char* type) {
    sendLoRaEvent(type, nullptr);
}

// ============================================================
// ESP-NOW - ENVOI
// ============================================================
void espnowBroadcast(GamePacket* pkt) {
    uint8_t bc[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_now_send(bc, (uint8_t*)pkt, sizeof(GamePacket));
}

void espnowSend(const uint8_t* mac, GamePacket* pkt) {
    esp_now_send(mac, (uint8_t*)pkt, sizeof(GamePacket));
}

void broadcastPing() {
    GamePacket pkt = {0};
    pkt.type = PKT_PING;
    espnowBroadcast(&pkt);
}

void assignSatellite(int idx) {
    if (idx < 0 || idx >= satCount) return;
    SatelliteInfo& s = satellites[idx];
    GamePacket pkt   = {0};
    pkt.type    = PKT_ASSIGN;
    pkt.sat_id  = idx;
    pkt.team    = s.team;
    pkt.role    = s.role;
    strncpy(pkt.player, s.player, sizeof(pkt.player) - 1);

    // Enregistre le peer si besoin
    if (!esp_now_is_peer_exist(s.mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, s.mac, 6);
        peer.channel = ESPNOW_CHANNEL;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
    espnowSend(s.mac, &pkt);
    s.assigned = true;
    Serial.printf("[ASSIGN] Satellite %d : %s equipe=%d role=%d\n",
                  idx, s.player, s.team, s.role);
}

void startGame() {
    GamePacket pkt = {0};
    pkt.type = PKT_GAME_START;
    espnowBroadcast(&pkt);
    sendLoRaGameEvent("game_start");
    // Enregistrement de chaque joueur aupres du serveur terrain
    for (int i = 0; i < satCount; i++) {
        sendLoRaEvent("player_register", &satellites[i]);
        delay(100); // Laisser le temps au serveur terrain de traiter
    }
}

void endGame() {
    GamePacket pkt = {0};
    pkt.type = PKT_GAME_END;
    espnowBroadcast(&pkt);
    sendLoRaGameEvent("game_end");
}

void reviveSatellite(int idx) {
    if (idx < 0 || idx >= satCount) return;
    SatelliteInfo& s = satellites[idx];
    GamePacket pkt = {0};
    pkt.type   = PKT_REVIVE;
    pkt.sat_id = idx;
    espnowSend(s.mac, &pkt);
    s.status   = 0;
    sendLoRaEvent("revive", &s);
    Serial.printf("[REVIVE] Satellite %d (%s) revive (RSSI=%d)\n",
                  idx, s.player, s.lastRssi);
}

// ============================================================
// ESP-NOW - RECEPTION
// ============================================================
// Pointeur pour stocker le RSSI recu dans la callback
static int8_t _lastRssi = 0;
static uint8_t _lastSrcMac[6] = {0};

void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < (int)sizeof(GamePacket)) return;
    const GamePacket* pkt = (const GamePacket*)data;

    int8_t  rssi    = info->rx_ctrl->rssi;
    const uint8_t* srcMac = info->src_addr;

    switch (pkt->type) {

        case PKT_ACK: {
            // Nouveau satellite - l'enregistrer si pas deja vu
            bool found = false;
            for (int i = 0; i < satCount; i++) {
                if (memcmp(satellites[i].mac, srcMac, 6) == 0) {
                    found = true; break;
                }
            }
            if (!found && satCount < MAX_SATELLITES) {
                int idx = satCount++;
                memset(&satellites[idx], 0, sizeof(SatelliteInfo));
                memcpy(satellites[idx].mac, srcMac, 6);
                satellites[idx].registered = true;
                satellites[idx].lastSeen   = millis();
                satellites[idx].lastRssi   = rssi;
                snprintf(satellites[idx].player, sizeof(satellites[idx].player),
                         "Joueur %d", idx + 1);
                satellites[idx].team = (idx % (TEAM_COUNT - 1)) + 1;
                satellites[idx].role = 0;
                // Enregistrer peer ESP-NOW
                if (!esp_now_is_peer_exist(srcMac)) {
                    esp_now_peer_info_t peer = {0};
                    memcpy(peer.peer_addr, srcMac, 6);
                    peer.channel = ESPNOW_CHANNEL;
                    peer.encrypt = false;
                    esp_now_add_peer(&peer);
                }
                Serial.printf("[ACK] Nouveau satellite %d detecte\n", idx);
            }
            break;
        }

        case PKT_HEARTBEAT: {
            uint8_t id = pkt->sat_id;
            if (id >= satCount) break;
            satellites[id].lastSeen  = millis();
            satellites[id].lastRssi  = rssi;

            // Detection proximite pour revive (~1m)
            if (gState == STATE_GAME &&
                satellites[id].status == 1 &&   // satellite est OUT
                rssi >= PROXIMITY_RSSI)          // assez proche
            {
                reviveSatellite(id);
            }
            break;
        }

        case PKT_BUTTON: {
            uint8_t id = pkt->sat_id;
            if (id >= satCount) break;
            if (gState == STATE_GAME && satellites[id].status == 0) {
                satellites[id].status = 1;
                satellites[id].deaths = pkt->deaths;
                sendLoRaEvent("death", &satellites[id]);
                Serial.printf("[MORT] %s (#%d) | Total: %lu\n",
                              satellites[id].player, id,
                              (unsigned long)satellites[id].deaths);
                // Feedback LED sur la valise
                setLEDs(0xFF0000);
                delay(200);
                setLEDs(0);
            }
            break;
        }
    }
}

void onSent(const uint8_t* mac, esp_now_send_status_t status) {}

// ============================================================
// LORA INIT
// ============================================================
bool initLoRa() {
    int state = lora.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                           RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_POWER, LORA_PREAMBLE);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] OK - %.1f MHz SF%d BW%.0fkHz\n",
                      LORA_FREQ, LORA_SF, LORA_BW);
        return true;
    }
    Serial.printf("[LoRa] Erreur init: %d\n", state);
    return false;
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Brigade des Sangliers - Valise ===");

    // LED
    strip.begin();
    strip.setBrightness(LED_BRIGHTNESS);
    setLEDs(0x200010);
    delay(400);
    setLEDs(0);

    // Boutons
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);

    // Clavier
    kbdSetup();

    // OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[ERREUR] OLED non trouve!");
    }
    oled.setTextColor(SSD1306_WHITE);
    oled.clearDisplay();
    oled.setTextSize(1); oled.setCursor(5, 10);  oled.println("Brigade des");
    oled.setTextSize(2); oled.setCursor(0, 24);  oled.println("Sangliers");
    oled.setTextSize(1); oled.setCursor(10, 50); oled.println("Valise v1.0");
    oled.display();
    delay(1500);

    // LoRa SX1262 (radio separee du WiFi - pas d'interference)
    loraOk = initLoRa();
    if (!loraOk) {
        oled.clearDisplay();
        oled.setCursor(0, 20); oled.println("LORA ERREUR!");
        oled.setCursor(0, 34); oled.println("Verif cablage SX1262");
        oled.display();
        delay(3000);
    }

    // ESP-NOW (WiFi STA mode - canal fixe)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERREUR] Echec init ESP-NOW!");
        while (true) delay(1000);
    }
    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSent);

    // Peer broadcast pour PING
    esp_now_peer_info_t bc = {0};
    memset(bc.peer_addr, 0xFF, 6);
    bc.channel = ESPNOW_CHANNEL;
    bc.encrypt = false;
    esp_now_add_peer(&bc);

    Serial.printf("[INFO] MAC valise: %s | LoRa: %s\n",
                  WiFi.macAddress().c_str(), loraOk ? "OK" : "ERREUR");
    gState = STATE_DISCOVERY;
}

// ============================================================
// BOUCLE PRINCIPALE
// ============================================================
void loop() {
    uint32_t now = millis();
    char key     = kbdScan();
    bool btnA    = (digitalRead(BTN_A_PIN) == LOW && now - lastBtnA > DEBOUNCE_MS);
    bool btnB    = (digitalRead(BTN_B_PIN) == LOW && now - lastBtnB > DEBOUNCE_MS);
    if (btnA) lastBtnA = now;
    if (btnB) lastBtnB = now;

    switch (gState) {

        // ============================================================
        case STATE_DISCOVERY:
            // Ping periodique pour decouvrir les satellites
            if (now - lastPing > PING_INTERVAL_MS) {
                lastPing = now;
                broadcastPing();
            }
            // Indicateur LED : bleu pulsant
            {
                float v = (sin(now / 800.0 * PI) + 1.0) / 2.0;
                uint8_t b = 80 * v;
                strip.fill(strip.Color(0, 0, b));
                strip.show();
            }
            if (now - lastDisplay > 500) {
                lastDisplay = now;
                showDiscovery();
            }
            // [A] = passer en config
            if (btnA && satCount > 0) {
                menuSat    = 0;
                inputField = 0;
                inputLen   = 0;
                memset(inputBuf, 0, sizeof(inputBuf));
                teamSel = satellites[0].team;
                roleSel = satellites[0].role;
                gState  = STATE_CONFIG;
            }
            // [B] = reset tous les satellites
            if (btnB) {
                GamePacket pkt = {0};
                pkt.type = PKT_RESET;
                espnowBroadcast(&pkt);
                satCount = 0;
                memset(satellites, 0, sizeof(satellites));
            }
            break;

        // ============================================================
        case STATE_CONFIG:
            if (now - lastDisplay > 100) {
                lastDisplay = now;
                showConfigMenu();
            }
            // Saisie clavier selon le champ actif
            if (key) {
                if (inputField == 0) {
                    // Saisie nom : T9
                    if (key == 'A') {
                        // Valider le nom, passer a equipe
                        strncpy(satellites[menuSat].player, inputBuf, 23);
                        teamSel    = satellites[menuSat].team;
                        inputField = 1;
                    } else {
                        handleNameInput(key);
                    }
                } else if (inputField == 1) {
                    // Selection equipe
                    if (key == 'C') {
                        teamSel = (teamSel % (TEAM_COUNT - 1)) + 1;
                    } else if (key == 'A') {
                        satellites[menuSat].team = teamSel;
                        roleSel    = satellites[menuSat].role;
                        inputField = 2;
                    } else if (key == 'B') {
                        inputField = 0;
                    }
                } else if (inputField == 2) {
                    // Selection role
                    if (key == 'D') {
                        roleSel = (roleSel + 1) % ROLE_COUNT;
                    } else if (key == 'A') {
                        satellites[menuSat].role = roleSel;
                        // Assignation au satellite et passage au suivant
                        assignSatellite(menuSat);
                        menuSat++;
                        if (menuSat >= satCount) {
                            // Tous configures - afficher option demarrer
                            menuSat    = 0;
                            inputField = 0;
                        } else {
                            // Satellite suivant
                            strncpy(inputBuf, satellites[menuSat].player, 23);
                            inputLen   = strlen(inputBuf);
                            teamSel    = satellites[menuSat].team;
                            roleSel    = satellites[menuSat].role;
                            inputField = 0;
                        }
                    } else if (key == 'B') {
                        inputField = 1;
                    }
                }
            }
            // [A] long (via bouton physique) = demarrer la partie
            if (btnA) {
                // Assigner tous les non-assignes avec valeurs par defaut
                for (int i = 0; i < satCount; i++) {
                    if (!satellites[i].assigned) assignSatellite(i);
                }
                delay(500); // Laisser le temps aux satellites de recevoir
                startGame();
                gState = STATE_GAME;
                setLEDs(0x00FF00); // Vert = partie lancee
                delay(300);
                setLEDs(0);
            }
            // [B] = retour decouverte
            if (btnB) gState = STATE_DISCOVERY;
            break;

        // ============================================================
        case STATE_GAME:
            if (now - lastDisplay > 500) {
                lastDisplay = now;
                showGameScreen();
            }
            // Detection satellites trop anciens (hors portee)
            for (int i = 0; i < satCount; i++) {
                if (satellites[i].registered &&
                    now - satellites[i].lastSeen > SATELLITE_TIMEOUT_MS) {
                    // Satellite injoignable depuis trop longtemps
                    // (on garde le statut actuel, on log juste)
                }
            }
            // [B] = fin de partie
            if (btnB) {
                endGame();
                gState = STATE_RESULTS;
                setLEDs(0xFF8800);
                delay(500);
                setLEDs(0);
            }
            break;

        // ============================================================
        case STATE_RESULTS:
            if (now - lastDisplay > 500) {
                lastDisplay = now;
                showResults();
            }
            // [A] = nouvelle partie (retour decouverte)
            if (btnA) {
                GamePacket pkt = {0};
                pkt.type = PKT_RESET;
                espnowBroadcast(&pkt);
                delay(300);
                satCount = 0;
                memset(satellites, 0, sizeof(satellites));
                gState   = STATE_DISCOVERY;
                setLEDs(0x000080);
                delay(300);
                setLEDs(0);
            }
            break;
    }

    delay(20); // Limite la charge CPU
}

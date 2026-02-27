/*
 * ============================================================
 *  Brigade des Sangliers - Firmware Satellite
 *  Materiel : ESP32-C3
 *
 *  Chaque joueur porte un satellite. La valise (Heltec V4)
 *  fait office de hub central.
 *
 *  Comportement :
 *  - Phase config : affiche nom/equipe/role, LED couleur equipe
 *  - En jeu       : LED eteinte, bouton actif
 *  - Appui bouton : satellite clignote rouge, statut "ELIMINE"
 *  - Retour valise (<1m) : revive automatique par la valise
 *
 *  Librairies requises (Arduino Library Manager) :
 *    - Adafruit NeoPixel
 *    - Adafruit SSD1306
 *    - Adafruit GFX Library
 * ============================================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ============================================================
// PROTOCOLE (identique dans satellite.ino et suitcase.ino)
// ============================================================
#define PKT_PING        0x01  // Valise -> broadcast : decouverte
#define PKT_ACK         0x02  // Satellite -> valise : je suis la
#define PKT_ASSIGN      0x03  // Valise -> satellite : tes infos
#define PKT_GAME_START  0x04  // Valise -> tous : debut de partie
#define PKT_GAME_END    0x05  // Valise -> tous : fin de partie
#define PKT_BUTTON      0x06  // Satellite -> valise : je suis elimine
#define PKT_REVIVE      0x07  // Valise -> satellite : tu es revive
#define PKT_HEARTBEAT   0x08  // Satellite -> valise : signal de vie
#define PKT_RESET       0x09  // Valise -> satellite : remise a zero

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  sat_id;
    uint8_t  team;
    uint8_t  role;
    uint8_t  status;      // 0 = en vie, 1 = elimine
    uint32_t deaths;
    char     player[24];
} GamePacket;

// ============================================================
// ETATS DE LA MACHINE A ETATS
// ============================================================
enum GameState {
    STATE_INIT,       // Demarrage, attente de la valise
    STATE_PAIRING,    // Valise detectee, en attente d'assignation
    STATE_CONFIGURED, // Role/equipe assigne, affichage
    STATE_ACTIVE,     // Partie en cours, LED eteinte
    STATE_DEAD,       // Joueur elimine, clignotement rouge
    STATE_GAMEOVER    // Fin de partie
};

// ============================================================
// VARIABLES GLOBALES
// ============================================================
GameState   gState        = STATE_INIT;
GamePacket  myData        = {0};

uint8_t     suitcaseMAC[6] = {0};
bool        paired         = false;
esp_now_peer_info_t suitcasePeer;

uint32_t    lastHeartbeat  = 0;
uint32_t    lastPairRetry  = 0;
uint32_t    lastBlink      = 0;
uint32_t    lastAnim       = 0;
bool        blinkOn        = false;

volatile bool btnEvent    = false;
uint32_t      lastBtnTime = 0;

// ============================================================
// MATERIEL
// ============================================================
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306  oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================================
// HELPERS AFFICHAGE
// ============================================================
void showSplash() {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(8, 10);  oled.println("La Brigade des");
    oled.setCursor(20, 22); oled.println("Sangliers");
    oled.setTextSize(1);
    oled.setCursor(10, 42); oled.println("Satellite v1.0");
    oled.display();
}

void showStatus(const char* l1, const char* l2, const char* l3, const char* l4 = "") {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);

    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(l1);

    oled.setTextSize(2);
    oled.setCursor(0, 14);
    oled.println(l2);

    oled.setTextSize(1);
    oled.setCursor(0, 42);
    oled.println(l3);
    oled.setCursor(0, 54);
    oled.println(l4);

    oled.display();
}

void showPlayerInfo() {
    char teamLine[28], deathLine[28];
    const char* teamName = (myData.team < TEAM_COUNT) ? TEAM_NAMES[myData.team] : "?";
    const char* roleName = (myData.role < ROLE_COUNT) ? ROLE_NAMES[myData.role] : "?";
    snprintf(teamLine,  sizeof(teamLine),  "Equipe %s | %s", teamName, roleName);
    snprintf(deathLine, sizeof(deathLine), "Morts: %lu", (unsigned long)myData.deaths);
    showStatus(teamLine, myData.player, "EN JEU", deathLine);
}

void showDeadScreen() {
    char deathLine[28];
    snprintf(deathLine, sizeof(deathLine), "Morts: %lu", (unsigned long)myData.deaths);
    showStatus("!! ELIMINE !!", "Revenez", "a la valise", deathLine);
}

void showGameOver() {
    char deathLine[28];
    snprintf(deathLine, sizeof(deathLine), "Total morts: %lu", (unsigned long)myData.deaths);
    showStatus("FIN DE PARTIE", myData.player, deathLine, "");
}

// ============================================================
// HELPERS LED
// ============================================================
void setAllLEDs(uint32_t color) {
    strip.fill(color);
    strip.show();
}

// Effet de respiration pour la phase config
void breathEffect(uint32_t color) {
    float phase = (sin((millis() / 1200.0) * 2 * PI) + 1.0) / 2.0;
    uint8_t r = ((color >> 16) & 0xFF) * phase;
    uint8_t g = ((color >> 8)  & 0xFF) * phase;
    uint8_t b = ((color)       & 0xFF) * phase;
    strip.fill(strip.Color(r, g, b));
    strip.show();
}

// ============================================================
// HELPERS ESP-NOW
// ============================================================
void addBroadcastPeer() {
    esp_now_peer_info_t bp;
    memset(&bp, 0, sizeof(bp));
    memset(bp.peer_addr, 0xFF, 6);
    bp.channel = ESPNOW_CHANNEL;
    bp.encrypt = false;
    esp_now_add_peer(&bp);
}

void sendTo(const uint8_t* mac, GamePacket* pkt) {
    esp_now_send(mac, (uint8_t*)pkt, sizeof(GamePacket));
}

void sendACK() {
    GamePacket pkt = {0};
    pkt.type   = PKT_ACK;
    pkt.sat_id = myData.sat_id;
    uint8_t bc[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    sendTo(bc, &pkt);
}

void sendHeartbeat() {
    if (!paired) return;
    GamePacket pkt = {0};
    pkt.type   = PKT_HEARTBEAT;
    pkt.sat_id = myData.sat_id;
    pkt.status = myData.status;
    pkt.deaths = myData.deaths;
    sendTo(suitcaseMAC, &pkt);
}

void sendButtonPress() {
    if (!paired) return;
    GamePacket pkt = {0};
    pkt.type   = PKT_BUTTON;
    pkt.sat_id = myData.sat_id;
    pkt.status = 1;
    pkt.deaths = myData.deaths;
    sendTo(suitcaseMAC, &pkt);
}

// ============================================================
// CALLBACK ESP-NOW - RECEPTION
// ============================================================
void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < (int)sizeof(GamePacket)) return;
    const GamePacket* pkt = (const GamePacket*)data;

    switch (pkt->type) {

        case PKT_PING:
            if (!paired) {
                memcpy(suitcaseMAC, info->src_addr, 6);
                memset(&suitcasePeer, 0, sizeof(suitcasePeer));
                memcpy(suitcasePeer.peer_addr, suitcaseMAC, 6);
                suitcasePeer.channel = ESPNOW_CHANNEL;
                suitcasePeer.encrypt = false;
                esp_now_add_peer(&suitcasePeer);
                paired = true;
                Serial.println("[OK] Valise trouvee, envoi ACK");
            }
            sendACK();
            if (gState == STATE_INIT) gState = STATE_PAIRING;
            break;

        case PKT_ASSIGN:
            myData.sat_id = pkt->sat_id;
            myData.team   = pkt->team;
            myData.role   = pkt->role;
            myData.status = 0;
            myData.deaths = 0;
            strncpy(myData.player, pkt->player, sizeof(myData.player) - 1);
            myData.player[sizeof(myData.player) - 1] = '\0';
            gState = STATE_CONFIGURED;
            Serial.printf("[ASSIGN] Joueur=%s Equipe=%d Role=%d\n",
                          myData.player, myData.team, myData.role);
            break;

        case PKT_GAME_START:
            if (gState == STATE_CONFIGURED || gState == STATE_PAIRING) {
                myData.status = 0;
                gState = STATE_ACTIVE;
                Serial.println("[GAME] Partie commencee!");
            }
            break;

        case PKT_REVIVE:
            if (pkt->sat_id == myData.sat_id && gState == STATE_DEAD) {
                myData.status = 0;
                gState = STATE_ACTIVE;
                Serial.println("[REVIVE] Joueur revive!");
            }
            break;

        case PKT_GAME_END:
            gState = STATE_GAMEOVER;
            Serial.println("[GAME] Fin de partie!");
            break;

        case PKT_RESET:
            gState  = STATE_INIT;
            paired  = false;
            memset(&myData, 0, sizeof(myData));
            Serial.println("[RESET] Remise a zero");
            break;
    }
}

void onSent(const uint8_t* mac, esp_now_send_status_t status) {
    // Rien de particulier
}

// ============================================================
// ISR BOUTON
// ============================================================
void IRAM_ATTR onButton() {
    uint32_t now = millis();
    if (now - lastBtnTime > DEBOUNCE_MS) {
        btnEvent   = true;
        lastBtnTime = now;
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Brigade des Sangliers - Satellite ===");

    // LED strip
    strip.begin();
    strip.setBrightness(LED_BRIGHTNESS);
    setAllLEDs(0x200010); // Eclat orange au demarrage
    delay(400);
    setAllLEDs(0);

    // Bouton
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(BUTTON_PIN, onButton, FALLING);

    // OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[ERREUR] OLED non detecte!");
    }
    showSplash();
    delay(1500);

    // WiFi en mode STA (requis pour ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERREUR] Echec init ESP-NOW!");
        showStatus("ERREUR", "ESP-NOW", "Verifiez le", "materiel");
        while (true) delay(1000);
    }
    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSent);
    addBroadcastPeer();

    Serial.printf("[INFO] MAC: %s\n", WiFi.macAddress().c_str());
    showStatus("Recherche", "Valise...", "En attente...", "");
}

// ============================================================
// BOUCLE PRINCIPALE - MACHINE A ETATS
// ============================================================
void loop() {
    uint32_t now = millis();

    // Traitement evenement bouton
    if (btnEvent) {
        btnEvent = false;
        if (gState == STATE_ACTIVE) {
            myData.deaths++;
            myData.status = 1;
            sendButtonPress();
            gState = STATE_DEAD;
            Serial.printf("[BUTTON] Elimine! Total morts: %lu\n", (unsigned long)myData.deaths);
        }
    }

    switch (gState) {

        // ---- Attente du premier PING de la valise ----
        case STATE_INIT:
            if (now - lastBlink > 1000) {
                lastBlink = now;
                blinkOn = !blinkOn;
                setAllLEDs(blinkOn ? 0x000080 : 0); // Bleu lent = recherche
            }
            if (now - lastAnim > 500) {
                lastAnim = now;
                showStatus("Recherche", "Valise...", WiFi.macAddress().c_str(), "");
            }
            break;

        // ---- Valise detectee, attente assignation ----
        case STATE_PAIRING:
            if (now - lastBlink > 400) {
                lastBlink = now;
                blinkOn = !blinkOn;
                setAllLEDs(blinkOn ? 0x004000 : 0); // Vert rapide = connecte
            }
            if (now - lastPairRetry > PAIR_RETRY_MS) {
                lastPairRetry = now;
                sendACK();
            }
            showStatus("Connecte!", "Attente", "assignation...", "");
            break;

        // ---- Infos assignees, affichage couleur equipe ----
        case STATE_CONFIGURED:
            {
                uint32_t color = (myData.team < TEAM_COUNT) ?
                                 TEAM_COLORS[myData.team] : 0xFFFFFF;
                breathEffect(color);
            }
            if (now - lastAnim > 200) {
                lastAnim = now;
                showPlayerInfo();
            }
            break;

        // ---- Partie en cours, LED eteinte, bouton actif ----
        case STATE_ACTIVE:
            setAllLEDs(0); // LED eteinte
            if (now - lastAnim > 2000) {
                lastAnim = now;
                showPlayerInfo();
            }
            if (now - lastHeartbeat > HEARTBEAT_MS) {
                lastHeartbeat = now;
                sendHeartbeat();
            }
            break;

        // ---- Elimine : clignotement rouge + heartbeat ----
        case STATE_DEAD:
            if (now - lastBlink > BLINK_ON_MS) {
                lastBlink = now;
                blinkOn = !blinkOn;
                setAllLEDs(blinkOn ? COLOR_DEAD_BLINK : 0);
            }
            if (now - lastAnim > 500) {
                lastAnim = now;
                showDeadScreen();
            }
            if (now - lastHeartbeat > HEARTBEAT_MS) {
                lastHeartbeat = now;
                sendHeartbeat(); // La valise lit le RSSI pour le revive
            }
            break;

        // ---- Fin de partie ----
        case STATE_GAMEOVER:
            {
                uint32_t color = (myData.team < TEAM_COUNT) ?
                                 TEAM_COLORS[myData.team] : 0xFFFFFF;
                setAllLEDs(color);
            }
            showGameOver();
            break;
    }
}

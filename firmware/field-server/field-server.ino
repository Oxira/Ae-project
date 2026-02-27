/*
 * ============================================================
 *  Brigade des Sangliers - Firmware Serveur Terrain
 *  Materiel : Heltec WiFi LoRa 32 V4 (ESP32-S3)
 *
 *  Passerelle bidirectionnelle LoRa <-> MQTT :
 *    - RECEPTION  : paquets JSON de la valise via LoRa 868 MHz
 *    - EMISSION   : evenements publiés sur le broker MQTT
 *    - OPTIONNEL  : reception de commandes MQTT vers la valise
 *
 *  Librairies requises (Arduino Library Manager) :
 *    - RadioLib              (LoRa SX1262)
 *    - PubSubClient          (MQTT)
 *    - Adafruit SSD1306 + GFX (OLED)
 *    - ArduinoJson
 *
 *  Librairie Heltec : https://github.com/HelTecAutomation/Heltec_ESP32
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================
// MATERIEL
// ============================================================
SX1262        lora = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
Adafruit_SSD1306 oled(128, 64, &Wire, OLED_RST);

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ============================================================
// ETAT
// ============================================================
bool loraOk  = false;
bool wifiOk  = false;
bool mqttOk  = false;

uint32_t lastMqttRetry  = 0;
uint32_t lastDisplayUpd = 0;

// Compteurs pour l'affichage
uint32_t totalReceived  = 0;
uint32_t totalPublished = 0;
String   lastEvent      = "";

// ============================================================
// AFFICHAGE OLED
// ============================================================
void updateDisplay() {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);

    // Ligne 0 : titre
    oled.setCursor(0, 0);
    oled.println("Brigade - Srv Terrain");

    // Ligne 1 : statuts
    char status[24];
    snprintf(status, sizeof(status), "LoRa:%s WiFi:%s MQTT:%s",
             loraOk ? "OK" : "ERR",
             wifiOk ? "OK" : "NON",
             mqttOk ? "OK" : "NON");
    oled.setCursor(0, 12);
    oled.println(status);

    // Ligne 2 : compteurs
    char counters[24];
    snprintf(counters, sizeof(counters), "RX:%lu TX:%lu",
             (unsigned long)totalReceived, (unsigned long)totalPublished);
    oled.setCursor(0, 24);
    oled.println(counters);

    // Ligne 3 : dernier evenement recu
    oled.setCursor(0, 38);
    oled.println("Dernier evt:");
    oled.setCursor(0, 50);
    oled.println(lastEvent.length() > 21 ? lastEvent.substring(0, 21) : lastEvent);

    oled.display();
}

// ============================================================
// WIFI
// ============================================================
bool connectWiFi() {
    Serial.printf("[WiFi] Connexion a '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
        delay(300);
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connecte! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Echec - mode hors-ligne");
    return false;
}

void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        wifiOk = false;
        mqttOk = false;
        connectWiFi();
        wifiOk = (WiFi.status() == WL_CONNECTED);
    }
}

// ============================================================
// MQTT
// ============================================================

// Callback MQTT : commandes recues depuis le backend (optionnel)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Ici on pourrait relayer une commande vers la valise via LoRa
    // (ex: forcer fin de partie, ajuster parametres...)
    char msg[length + 1];
    memcpy(msg, payload, length);
    msg[length] = '\0';
    Serial.printf("[MQTT RX] topic=%s msg=%s\n", topic, msg);
    // TODO : transmettre la commande a la valise via LoRa si besoin
}

bool connectMQTT() {
    if (!wifiOk) return false;
    Serial.printf("[MQTT] Connexion a %s:%d...\n", MQTT_BROKER, MQTT_PORT);

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(512);

    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
    } else {
        connected = mqtt.connect(MQTT_CLIENT_ID);
    }

    if (connected) {
        Serial.println("[MQTT] Connecte!");
        // S'abonner aux commandes (optionnel)
        mqtt.subscribe(MQTT_TOPIC_COMMANDS);
        return true;
    } else {
        Serial.printf("[MQTT] Echec - etat: %d\n", mqtt.state());
        return false;
    }
}

void checkMQTT() {
    if (!mqtt.connected()) {
        mqttOk = false;
        uint32_t now = millis();
        if (now - lastMqttRetry > MQTT_RECONNECT_MS) {
            lastMqttRetry = now;
            mqttOk = connectMQTT();
        }
    }
}

// ============================================================
// PUBLICATION MQTT
// ============================================================
bool publishEvent(const char* jsonPayload, size_t len) {
    if (!mqttOk || !mqtt.connected()) return false;

    bool ok = mqtt.publish(MQTT_TOPIC_EVENTS, (const uint8_t*)jsonPayload, len, false);
    if (ok) {
        totalPublished++;
        Serial.printf("[MQTT TX] %s\n", MQTT_TOPIC_EVENTS);
    } else {
        Serial.println("[MQTT TX] Echec publication!");
    }
    return ok;
}

// ============================================================
// LORA - RECEPTION
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

// Tentative de reception d'un paquet LoRa (non-bloquant avec timeout court)
void tryReceiveLoRa() {
    if (!loraOk) return;

    // Mise en mode reception avec timeout
    // RadioLib : RADIOLIB_ERR_RX_TIMEOUT si rien recu
    uint8_t buf[256];
    int     state = lora.receive(buf, sizeof(buf));

    if (state == RADIOLIB_ERR_NONE) {
        // Paquet recu
        size_t len = lora.getPacketLength();
        buf[len < sizeof(buf) ? len : sizeof(buf) - 1] = '\0';

        float  rssi = lora.getRSSI();
        float  snr  = lora.getSNR();
        totalReceived++;

        Serial.printf("[LoRa RX] %d octets RSSI=%.0f SNR=%.1f : %s\n",
                      (int)len, rssi, snr, buf);

        // Enrichir le JSON avec les metriques LoRa
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, (char*)buf);

        if (err) {
            Serial.printf("[LoRa RX] JSON invalide: %s\n", err.c_str());
            return;
        }

        // Ajouter les metriques radio
        doc["lora_rssi"] = (int)rssi;
        doc["lora_snr"]  = snr;

        // Extraire le type pour l'affichage
        const char* type = doc["type"] | "unknown";
        lastEvent = String(type);
        if (doc.containsKey("player")) {
            lastEvent += " " + String(doc["player"].as<const char*>());
        }

        // Re-serialiser et publier via MQTT
        char out[256];
        size_t outLen = serializeJson(doc, out, sizeof(out));
        publishEvent(out, outLen);

    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // Normal : pas de paquet dans la fenetre de timeout
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[LoRa RX] Erreur CRC - paquet corrompu");
    } else {
        Serial.printf("[LoRa RX] Erreur: %d\n", state);
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Brigade des Sangliers - Serveur Terrain ===");

    // OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[ERREUR] OLED non detecte!");
    }
    oled.setTextColor(SSD1306_WHITE);
    oled.clearDisplay();
    oled.setTextSize(1); oled.setCursor(0, 10); oled.println("Brigade des Sangliers");
    oled.setTextSize(1); oled.setCursor(0, 24); oled.println("Serveur Terrain");
    oled.setTextSize(1); oled.setCursor(0, 38); oled.println("Initialisation...");
    oled.display();
    delay(1000);

    // LoRa
    loraOk = initLoRa();

    // WiFi
    wifiOk = connectWiFi();

    // MQTT
    if (wifiOk) {
        mqttOk = connectMQTT();
    }

    updateDisplay();
    Serial.printf("[INFO] Pret. LoRa=%s WiFi=%s MQTT=%s\n",
                  loraOk ? "OK" : "ERR",
                  wifiOk ? "OK" : "ERR",
                  mqttOk ? "OK" : "ERR");
}

// ============================================================
// BOUCLE PRINCIPALE
// ============================================================
void loop() {
    // Maintenir les connexions
    checkWiFi();
    checkMQTT();
    mqtt.loop();

    // Ecouter le LoRa (non-bloquant - timeout interne RadioLib)
    tryReceiveLoRa();

    // Rafraichir l'affichage toutes les 2 secondes
    uint32_t now = millis();
    if (now - lastDisplayUpd > 2000) {
        lastDisplayUpd = now;
        updateDisplay();
    }
}

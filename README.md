# La Brigade des Sangliers — Systeme de Suivi Airsoft

Systeme complet de gestion de parties airsoft en temps reel.
Chaque joueur porte un **satellite** (ESP32-C3), la **valise** (Heltec V4) est le hub central,
et le **site web** affiche les scores en direct via WebSocket.

---

## Architecture

```
                   ESP-NOW
 [Satellite 1]  ←──────────→
 [Satellite 2]  ←──────────→  [Valise Heltec V4]  ──WiFi/HTTP──→  [Serveur Node.js]  ←──WebSocket──  [Navigateur]
 [Satellite N]  ←──────────→
```

| Composant     | Materiel           | Role                                      |
|---------------|--------------------|-------------------------------------------|
| Satellite     | ESP32-C3           | Module joueur (LED, bouton, ecran OLED)   |
| Valise        | Heltec WiFi LoRa V4| Hub central, gestion de partie            |
| Serveur       | Raspberry Pi / PC  | API REST + WebSocket + site web           |
| Site web      | Navigateur         | Presentation equipe + scores temps reel   |

---

## Materiel requis

### Satellite (un par joueur)
| Composant           | Detail                          |
|---------------------|---------------------------------|
| ESP32-C3            | N'importe quelle carte ESP32-C3 |
| Ruban LED WS2812B   | 8 LEDs minimum                  |
| Bouton momentane    | Actif LOW, pull-up interne      |
| Ecran OLED SSD1306  | 128x64, I2C                     |

### Valise (une par terrain)
| Composant             | Detail                              |
|-----------------------|-------------------------------------|
| Heltec WiFi LoRa 32 V4| ESP32-S3, OLED integre              |
| Ruban LED WS2812B     | 16 LEDs                             |
| Clavier matriciel 4x4 | Keypad standard Arduino             |
| Module GPS (optionnel)| UART (TinyGPS++)                    |

---

## Cablage satellite (ESP32-C3)

```
ESP32-C3          Composant
─────────────     ─────────────────
GPIO10       →    DIN ruban WS2812B
GPIO9        →    Bouton (+ GND)
GPIO8 (SDA)  →    OLED SDA
GPIO7 (SCL)  →    OLED SCL
3.3V / GND       Alimentation
```

## Cablage valise (Heltec V4)

```
Heltec V4         Composant
─────────────     ─────────────────
GPIO38       →    DIN ruban WS2812B
GPIO0        →    Bouton A (BOOT)
GPIO3        →    Bouton B (PRG)
GPIO17 (SDA) →    OLED (integre)
GPIO18 (SCL) →    OLED (integre)
GPIO11-14    →    Rangees clavier (ROW0-3)
GPIO15-16,35,36 → Colonnes clavier (COL0-3)
GPIO44 (RX)  →    GPS TX
GPIO43 (TX)  →    GPS RX
```

---

## Librairies Arduino

Installez via Arduino Library Manager :

**Pour le satellite (ESP32-C3) :**
- `Adafruit NeoPixel`
- `Adafruit SSD1306`
- `Adafruit GFX Library`

**Pour la valise (Heltec V4) :**
- `Adafruit NeoPixel`
- `Adafruit SSD1306`
- `Adafruit GFX Library`
- `ArduinoJson`

**Board Manager :**
- ESP32 : `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Selectionner : `ESP32C3 Dev Module` (satellite) / `Heltec WiFi LoRa 32(V4)` (valise)

---

## Configuration reseau

### Sur le terrain (mode local)
1. Creez un hotspot WiFi sur votre smartphone ou routeur :
   - **SSID :** `BrigadeSangliers`
   - **Mot de passe :** `airsoft2024`
   - **Canal :** 1 (recommande)
2. Lancez le serveur sur un Raspberry Pi ou un PC connecte au meme reseau
3. La valise se connecte et envoie les evenements a `http://192.168.43.1:3000`

> Adaptez `SERVER_URL` dans `firmware/suitcase/config.h` selon l'IP de votre serveur.

---

## Installation du serveur

```bash
cd server
npm install
npm start
# Serveur disponible sur http://0.0.0.0:3000
```

Le serveur sert automatiquement le site web depuis le dossier `web/`.

### Variables d'environnement
```
PORT=3000   # Port d'ecoute (defaut: 3000)
```

---

## Fonctionnement d'une partie

### 1. Demarrage
1. Allumez les satellites → ils cherchent la valise (LED bleue clignotante)
2. Allumez la valise → elle diffuse des PINGs ESP-NOW
3. Les satellites repondent par ACK → la valise les enregistre

### 2. Configuration (valise)
1. La valise affiche le nombre de satellites detectes
2. Appuyez sur **Bouton A** pour passer en mode config
3. Pour chaque satellite :
   - Saisir le **nom** du joueur (clavier T9 : appuis multiples pour cycler les lettres)
   - **[A]** valide le nom → selectionner l'**equipe** ([C] pour changer)
   - **[A]** valide l'equipe → selectionner le **role** ([D] pour changer)
   - **[A]** valide et passe au satellite suivant
4. Quand tous les satellites sont configures, **Bouton A** physique demarre la partie

### 3. Partie en cours
- Les satellites affichent "EN JEU", LED eteinte
- **Appui bouton satellite** → LED clignote rouge, ecran "ELIMINE", statut "OUT"
- **Retour a la valise (<1m)** → la valise detecte la proximite (RSSI) et revive le joueur
- Chaque mort est comptabilisee et envoyee au serveur
- Le site web se met a jour en temps reel

### 4. Fin de partie
- **Bouton B** sur la valise → fin de partie
- Les satellites affichent les resultats finaux (couleur equipe)
- L'historique est sauvegarde sur le serveur
- **Bouton A** sur la valise → nouvelle partie

---

## API REST (valise → serveur)

| Methode | Endpoint            | Description                    |
|---------|---------------------|--------------------------------|
| POST    | `/api/event`        | Evenement de jeu               |
| GET     | `/api/state`        | Etat courant du jeu            |
| GET     | `/api/history`      | Historique des parties         |
| POST    | `/api/game/start`   | Demarrage manuel (admin)       |
| POST    | `/api/game/end`     | Fin manuelle (admin)           |
| POST    | `/api/game/reset`   | Remise a zero (admin)          |

### Format evenement POST /api/event
```json
{
  "type":         "death",
  "satellite_id": 2,
  "player":       "Jean-Paul",
  "team":         "Rouge",
  "role":         "Soldat",
  "deaths":       3,
  "status":       "out"
}
```
Types supportes : `player_register`, `death`, `revive`, `game_start`, `game_end`, `reset`

---

## Structure du projet

```
Ae-project/
├── firmware/
│   ├── satellite/
│   │   ├── satellite.ino   # Firmware ESP32-C3
│   │   └── config.h        # Pins et parametres satellite
│   └── suitcase/
│       ├── suitcase.ino    # Firmware Heltec V4
│       └── config.h        # Pins et parametres valise
├── server/
│   ├── package.json
│   └── server.js           # Serveur Node.js (API + WebSocket + site web)
├── web/
│   ├── index.html          # Site web (presentation + dashboard)
│   ├── css/style.css       # Styles
│   └── js/app.js           # Frontend JS (WebSocket + rendu)
├── .gitignore
├── LICENSE
└── README.md
```

---

## Calibration de la detection de proximite (~1m)

Le seuil RSSI est defini dans `firmware/suitcase/config.h` :
```c
#define PROXIMITY_RSSI  -55   // dBm
```

Pour calibrer :
1. Placez un satellite a exactement 1m de la valise
2. Observez les logs serie de la valise (`RSSI=...`)
3. Ajustez `PROXIMITY_RSSI` a une valeur legerement inferieure au RSSI mesure
4. Attendu : entre -45 et -65 dBm selon l'environnement

---

## Ajout du logo

1. Deposez votre logo dans `web/` sous le nom `logo.png`
2. Editez `web/index.html` :
   - Remplacez `<div class="hero-logo-placeholder">🐗</div>` par :
     ```html
     <img class="hero-logo" src="logo.png" alt="Brigade des Sangliers">
     ```
   - Faites de meme pour la nav (`.nav-brand`)

---

## Licence

GPL v3 — Voir [LICENSE](LICENSE)

**Brigade des Sangliers** — Airsoft Niort, Deux-Sevres 🐗

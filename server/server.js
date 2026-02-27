/**
 * ============================================================
 *  Brigade des Sangliers - Serveur de jeu temps reel
 *
 *  Fonctionne en local sur le terrain (Raspberry Pi / PC)
 *  ou en cloud (VPS) pour la diffusion des resultats.
 *
 *  Endpoints REST (pour la valise Heltec V4) :
 *    POST /api/event         -- evenement de jeu
 *    GET  /api/state         -- etat courant du jeu
 *    GET  /api/history       -- historique des parties
 *
 *  WebSocket (pour le site web / navigateurs) :
 *    emit 'game_update'      -- mise a jour temps reel
 *
 *  Usage : node server.js [port]
 *  Port par defaut : 3000
 * ============================================================
 */

const express    = require('express');
const http       = require('http');
const { Server } = require('socket.io');
const cors       = require('cors');
const bodyParser = require('body-parser');
const path       = require('path');
const fs         = require('fs');

const PORT = process.env.PORT || parseInt(process.argv[2]) || 3000;

// ============================================================
// INITIALISATION
// ============================================================
const app    = express();
const server = http.createServer(app);
const io     = new Server(server, {
    cors: { origin: '*', methods: ['GET', 'POST'] }
});

app.use(cors());
app.use(bodyParser.json());

// Servir le site web statique depuis ../web
const webDir = path.join(__dirname, '..', 'web');
app.use(express.static(webDir));

// ============================================================
// ETAT DU JEU (en memoire)
// ============================================================

/**
 * Structure player :
 * {
 *   satellite_id : number,
 *   player       : string,
 *   team         : string ('Rouge','Bleu',...),
 *   role         : string ('Soldat','Medic',...),
 *   deaths       : number,
 *   status       : 'alive' | 'out',
 *   joinedAt     : timestamp
 * }
 */
let gameState = {
    status    : 'idle',   // idle | running | ended
    name      : 'Partie 1',
    startTime : null,
    endTime   : null,
    players   : {}        // satellite_id -> player object
};

// Historique des parties
const HISTORY_FILE = path.join(__dirname, 'history.json');
let history = [];

function loadHistory() {
    try {
        if (fs.existsSync(HISTORY_FILE)) {
            history = JSON.parse(fs.readFileSync(HISTORY_FILE, 'utf8'));
        }
    } catch (e) {
        history = [];
    }
}

function saveHistory() {
    try {
        fs.writeFileSync(HISTORY_FILE, JSON.stringify(history, null, 2));
    } catch (e) {
        console.error('[HISTORY] Erreur sauvegarde:', e.message);
    }
}

loadHistory();

// ============================================================
// BROADCAST WEBSOCKET
// ============================================================
function broadcastUpdate() {
    io.emit('game_update', gameState);
}

// ============================================================
// TRAITEMENT DES EVENEMENTS
// ============================================================
function processEvent(event) {
    const { type, satellite_id, player, team, role, deaths, status } = event;

    console.log(`[EVENT] type=${type} sat=${satellite_id} player=${player || ''}`);

    switch (type) {

        case 'player_register':
            gameState.players[satellite_id] = {
                satellite_id,
                player : player || `Joueur ${satellite_id}`,
                team   : team   || 'unknown',
                role   : role   || 'Soldat',
                deaths : 0,
                status : 'alive',
                joinedAt : Date.now()
            };
            break;

        case 'death':
            if (gameState.players[satellite_id]) {
                gameState.players[satellite_id].deaths = (deaths !== undefined)
                    ? deaths
                    : (gameState.players[satellite_id].deaths + 1);
                gameState.players[satellite_id].status = 'out';
            }
            break;

        case 'revive':
            if (gameState.players[satellite_id]) {
                gameState.players[satellite_id].status = 'alive';
            }
            break;

        case 'game_start':
            gameState.status    = 'running';
            gameState.startTime = Date.now();
            gameState.endTime   = null;
            console.log('[GAME] Partie demarree!');
            break;

        case 'game_end':
            gameState.status  = 'ended';
            gameState.endTime = Date.now();
            // Archiver dans l'historique
            history.unshift({
                name      : gameState.name,
                startTime : gameState.startTime,
                endTime   : gameState.endTime,
                duration  : gameState.endTime - gameState.startTime,
                players   : Object.values(gameState.players)
            });
            if (history.length > 50) history.pop(); // Garder 50 parties max
            saveHistory();
            console.log('[GAME] Partie terminee, historique sauvegarde.');
            break;

        case 'reset':
            gameState = {
                status    : 'idle',
                name      : `Partie ${history.length + 2}`,
                startTime : null,
                endTime   : null,
                players   : {}
            };
            break;

        default:
            console.warn(`[EVENT] Type inconnu: ${type}`);
    }

    broadcastUpdate();
}

// ============================================================
// ROUTES REST
// ============================================================

// POST /api/event  -- appele par la valise Heltec
app.post('/api/event', (req, res) => {
    try {
        processEvent(req.body);
        res.json({ ok: true, timestamp: Date.now() });
    } catch (err) {
        console.error('[API] Erreur /api/event:', err);
        res.status(500).json({ ok: false, error: err.message });
    }
});

// GET /api/state  -- etat courant
app.get('/api/state', (req, res) => {
    res.json(gameState);
});

// GET /api/history  -- historique des parties
app.get('/api/history', (req, res) => {
    res.json(history.slice(0, 20)); // 20 dernieres parties
});

// GET /api/history/:idx  -- detail d'une partie
app.get('/api/history/:idx', (req, res) => {
    const idx = parseInt(req.params.idx);
    if (idx >= 0 && idx < history.length) {
        res.json(history[idx]);
    } else {
        res.status(404).json({ error: 'Partie introuvable' });
    }
});

// POST /api/game/reset  -- remise a zero manuelle (admin)
app.post('/api/game/reset', (req, res) => {
    processEvent({ type: 'reset' });
    res.json({ ok: true });
});

// POST /api/game/start  -- demarrage manuel (admin web)
app.post('/api/game/start', (req, res) => {
    const { name } = req.body || {};
    if (name) gameState.name = name;
    processEvent({ type: 'game_start' });
    res.json({ ok: true });
});

// POST /api/game/end  -- fin manuelle (admin web)
app.post('/api/game/end', (req, res) => {
    processEvent({ type: 'game_end' });
    res.json({ ok: true });
});

// Toute autre route -> index.html (SPA)
app.get('*', (req, res) => {
    res.sendFile(path.join(webDir, 'index.html'));
});

// ============================================================
// WEBSOCKET
// ============================================================
io.on('connection', (socket) => {
    console.log(`[WS] Nouveau client connecte: ${socket.id}`);
    // Envoyer l'etat courant immediatement
    socket.emit('game_update', gameState);

    socket.on('disconnect', () => {
        console.log(`[WS] Client deconnecte: ${socket.id}`);
    });

    // Commandes admin via WebSocket (optionnel)
    socket.on('admin_event', (event) => {
        processEvent(event);
    });
});

// ============================================================
// DEMARRAGE
// ============================================================
server.listen(PORT, '0.0.0.0', () => {
    console.log('╔══════════════════════════════════════╗');
    console.log('║  Brigade des Sangliers - Serveur     ║');
    console.log(`║  Ecoute sur le port ${PORT}             ║`);
    console.log('╠══════════════════════════════════════╣');
    console.log(`║  Site web  : http://localhost:${PORT}   ║`);
    console.log(`║  API state : http://localhost:${PORT}/api/state ║`);
    console.log('╚══════════════════════════════════════╝');
});

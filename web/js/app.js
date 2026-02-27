/**
 * Brigade des Sangliers - Frontend Application
 * Connexion WebSocket + affichage temps reel
 */

// ============================================================
// CONFIGURATION
// ============================================================
const SERVER_URL = window.location.origin; // Meme serveur que la page

// ============================================================
// ETAT LOCAL
// ============================================================
let gameState   = null;
let socket      = null;
let connected   = false;
let startTime   = null;
let timerInterval = null;

// ============================================================
// REFERENCES DOM
// ============================================================
const connDot        = document.getElementById('conn-dot');
const connText       = document.getElementById('conn-text');
const gameStatusBadge = document.getElementById('game-status-badge');
const playersGrid    = document.getElementById('players-grid');
const historyList    = document.getElementById('history-list');

// Quick stats
const qsTotal  = document.getElementById('qs-total');
const qsAlive  = document.getElementById('qs-alive');
const qsDeaths = document.getElementById('qs-deaths');
const qsTimer  = document.getElementById('qs-timer');

// ============================================================
// CONNEXION WEBSOCKET
// ============================================================
function connect() {
    // socket.io est charge depuis le serveur
    if (typeof io === 'undefined') {
        console.warn('[WS] socket.io non disponible - mode statique');
        fetchState();
        return;
    }

    socket = io(SERVER_URL, { transports: ['websocket', 'polling'] });

    socket.on('connect', () => {
        connected = true;
        setConnStatus(true);
        console.log('[WS] Connecte au serveur');
    });

    socket.on('disconnect', () => {
        connected = false;
        setConnStatus(false);
        console.log('[WS] Deconnecte');
    });

    socket.on('game_update', (data) => {
        gameState = data;
        render();
    });

    socket.on('connect_error', () => {
        connected = false;
        setConnStatus(false);
    });
}

// Fallback polling si pas de WebSocket
async function fetchState() {
    try {
        const res = await fetch('/api/state');
        if (res.ok) {
            gameState = await res.json();
            render();
        }
    } catch (e) {
        console.warn('[API] Serveur inaccessible');
    }
}

// ============================================================
// UI
// ============================================================
function setConnStatus(ok) {
    if (!connDot || !connText) return;
    if (ok) {
        connDot.className  = 'conn-dot live';
        connText.textContent = 'Connecte - Temps reel';
    } else {
        connDot.className  = 'conn-dot error';
        connText.textContent = 'Hors ligne';
    }
}

function teamClass(teamName) {
    if (!teamName) return '';
    const map = { rouge: 'team-rouge', bleu: 'team-bleu', vert: 'team-vert', jaune: 'team-jaune' };
    return map[teamName.toLowerCase()] || '';
}

function teamColor(teamName) {
    const map = {
        rouge: 'var(--team-red)',
        bleu:  'var(--team-blue)',
        vert:  'var(--team-green)',
        jaune: 'var(--team-yellow)'
    };
    return (teamName && map[teamName.toLowerCase()]) || 'var(--text-dim)';
}

function formatDuration(ms) {
    if (!ms || ms <= 0) return '--:--';
    const s = Math.floor(ms / 1000);
    const m = Math.floor(s / 60);
    const h = Math.floor(m / 60);
    if (h > 0) return `${h}h${String(m % 60).padStart(2,'0')}`;
    return `${String(m).padStart(2,'0')}:${String(s % 60).padStart(2,'0')}`;
}

function formatDate(ts) {
    if (!ts) return '';
    return new Date(ts).toLocaleString('fr-FR', {
        day: '2-digit', month: '2-digit', year: 'numeric',
        hour: '2-digit', minute: '2-digit'
    });
}

// ============================================================
// RENDU PRINCIPAL
// ============================================================
function render() {
    if (!gameState) return;

    renderStatusBadge();
    renderQuickStats();
    renderPlayers();
    updateTimer();
}

function renderStatusBadge() {
    if (!gameStatusBadge) return;
    const { status } = gameState;
    const labels = { idle: 'En attente', running: 'Partie en cours', ended: 'Partie terminee' };
    const icons  = { idle: '⏸', running: '🔴', ended: '🏁' };
    gameStatusBadge.textContent = `${icons[status] || ''} ${labels[status] || status}`;
    gameStatusBadge.className   = `game-status-badge ${status}`;
}

function renderQuickStats() {
    const players = Object.values(gameState.players || {});
    const alive  = players.filter(p => p.status === 'alive').length;
    const deaths = players.reduce((acc, p) => acc + (p.deaths || 0), 0);

    if (qsTotal)  qsTotal.textContent  = players.length;
    if (qsAlive)  qsAlive.textContent  = alive;
    if (qsDeaths) qsDeaths.textContent = deaths;
}

function renderPlayers() {
    if (!playersGrid) return;

    const players = Object.values(gameState.players || {});

    if (players.length === 0) {
        playersGrid.innerHTML = `
            <div class="empty-state">
                <div class="es-icon">🎮</div>
                <p>Aucun joueur enregistre.<br>Demarrez une partie depuis la valise.</p>
            </div>`;
        return;
    }

    // Trier : vivants d'abord, puis par nombre de morts
    players.sort((a, b) => {
        if (a.status === b.status) return (a.deaths || 0) - (b.deaths || 0);
        return a.status === 'alive' ? -1 : 1;
    });

    playersGrid.innerHTML = players.map(p => {
        const isOut    = p.status === 'out';
        const tClass   = teamClass(p.team);
        const tColor   = teamColor(p.team);
        const deathClr = p.deaths === 0 ? 'var(--text-soft)' : (p.deaths < 3 ? 'var(--orange)' : 'var(--out-color)');

        return `
        <div class="player-card ${isOut ? 'is-out' : ''}" style="--team-color:${tColor}">
            <div class="pc-header">
                <div class="pc-name">${escHtml(p.player)}</div>
                <div class="pc-team-badge ${tClass}">${escHtml(p.team || '---')}</div>
            </div>
            <div class="pc-meta">
                <span class="pc-role">${escHtml(p.role || 'Soldat')}</span>
                <span class="pc-deaths" style="color:${deathClr}">
                    ${p.deaths || 0} mort${(p.deaths || 0) !== 1 ? 's' : ''}
                </span>
            </div>
            <div class="pc-status ${isOut ? 'status-out' : 'status-alive'}">
                <div class="pc-status-dot"></div>
                ${isOut ? 'ELIMINE' : 'EN JEU'}
            </div>
        </div>`;
    }).join('');
}

function updateTimer() {
    if (!qsTimer) return;
    if (gameState.status === 'running' && gameState.startTime) {
        const elapsed = Date.now() - gameState.startTime;
        qsTimer.textContent = formatDuration(elapsed);
    } else if (gameState.status === 'ended' && gameState.startTime && gameState.endTime) {
        qsTimer.textContent = formatDuration(gameState.endTime - gameState.startTime);
    } else {
        qsTimer.textContent = '--:--';
    }
}

// ============================================================
// HISTORIQUE
// ============================================================
async function loadHistory() {
    if (!historyList) return;
    try {
        const res  = await fetch('/api/history');
        const data = await res.json();
        renderHistory(data);
    } catch (e) {
        historyList.innerHTML = '<p style="color:var(--text-dim);text-align:center">Historique indisponible</p>';
    }
}

function renderHistory(data) {
    if (!historyList) return;
    if (!data || data.length === 0) {
        historyList.innerHTML = '<p style="color:var(--text-dim);text-align:center;padding:3rem">Aucune partie jouee pour l\'instant.</p>';
        return;
    }

    historyList.innerHTML = data.map((game, idx) => {
        const dur     = game.duration ? formatDuration(game.duration) : '--';
        const players = (game.players || []).slice(0, 8);

        return `
        <div class="history-card">
            <div class="hc-header">
                <span class="hc-name">${escHtml(game.name || `Partie ${idx + 1}`)}</span>
                <span class="hc-date">${formatDate(game.startTime)}</span>
                <span class="hc-dur">⏱ ${dur}</span>
            </div>
            <div class="hc-players">
                ${players.map(p => `
                    <span class="hc-pill ${teamClass(p.team)}">
                        ${escHtml(p.player)} — ${p.deaths || 0} mort${(p.deaths||0)!==1?'s':''}
                    </span>`).join('')}
                ${(game.players||[]).length > 8 ? `<span class="hc-pill">+${(game.players.length-8)} autres</span>` : ''}
            </div>
        </div>`;
    }).join('');
}

// ============================================================
// UTILITAIRES
// ============================================================
function escHtml(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

function showToast(msg) {
    const t = document.getElementById('toast');
    if (!t) return;
    t.textContent = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 3000);
}

// Navigation active
function updateActiveNav() {
    const sections = document.querySelectorAll('section[id]');
    const links    = document.querySelectorAll('.nav-links a');
    const scrollY  = window.scrollY + 80;

    sections.forEach(sec => {
        if (scrollY >= sec.offsetTop && scrollY < sec.offsetTop + sec.offsetHeight) {
            links.forEach(l => {
                l.classList.toggle('active', l.getAttribute('href') === `#${sec.id}`);
            });
        }
    });
}

// Compte a rebours en direct
setInterval(() => {
    if (gameState && gameState.status === 'running') {
        updateTimer();
    }
}, 1000);

// Polling fallback toutes les 5s si WebSocket non disponible
setInterval(() => {
    if (!connected && typeof io === 'undefined') fetchState();
}, 5000);

// ============================================================
// INIT
// ============================================================
document.addEventListener('DOMContentLoaded', () => {
    connect();
    loadHistory();
    window.addEventListener('scroll', updateActiveNav, { passive: true });
    // Afficher la date de rechargement de l'historique
    setInterval(loadHistory, 30000);
});

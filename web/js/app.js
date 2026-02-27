/**
 * Brigade des Sangliers - Frontend Application
 * WebSocket STOMP (Spring Boot) + affichage temps reel
 */

// ============================================================
// CONFIGURATION
// ============================================================
const WS_ENDPOINT = window.location.origin + '/ws';    // Endpoint STOMP SockJS
const WS_TOPIC    = '/topic/game-update';              // Topic Spring STOMP

// ============================================================
// ETAT LOCAL
// ============================================================
let gameState    = null;
let stompClient  = null;
let connected    = false;

// ============================================================
// REFERENCES DOM
// ============================================================
const connDot         = document.getElementById('conn-dot');
const connText        = document.getElementById('conn-text');
const gameStatusBadge = document.getElementById('game-status-badge');
const playersGrid     = document.getElementById('players-grid');
const historyList     = document.getElementById('history-list');
const qsTotal         = document.getElementById('qs-total');
const qsAlive         = document.getElementById('qs-alive');
const qsDeaths        = document.getElementById('qs-deaths');
const qsTimer         = document.getElementById('qs-timer');

// ============================================================
// CONNEXION STOMP / SOCKJS
// ============================================================
function connect() {
    // StompJs est charge depuis le CDN (window.StompJs)
    if (typeof StompJs === 'undefined' && typeof Stomp === 'undefined') {
        console.warn('[WS] StompJs non disponible - polling REST');
        fetchState();
        return;
    }

    const SClient = window.StompJs ? window.StompJs.Client : window.Stomp.over;

    stompClient = new StompJs.Client({
        webSocketFactory: () => new SockJS(WS_ENDPOINT),
        reconnectDelay: 5000,       // Reconnexion auto apres 5s

        onConnect: () => {
            connected = true;
            setConnStatus(true);
            console.log('[WS] Connecte au serveur STOMP');

            stompClient.subscribe(WS_TOPIC, (msg) => {
                try {
                    gameState = JSON.parse(msg.body);
                    render();
                } catch (e) {
                    console.error('[WS] Parsing JSON:', e);
                }
            });

            // Charger l'etat initial via REST
            fetchState();
        },

        onDisconnect: () => {
            connected = false;
            setConnStatus(false);
            console.log('[WS] Deconnecte');
        },

        onStompError: (frame) => {
            connected = false;
            setConnStatus(false);
            console.error('[WS] Erreur STOMP :', frame.headers?.message);
        },

        onWebSocketClose: () => {
            connected = false;
            setConnStatus(false);
        }
    });

    stompClient.activate();
}

// Fallback REST (quand WS pas encore etabli)
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
// UI - STATUT CONNEXION
// ============================================================
function setConnStatus(ok) {
    if (!connDot || !connText) return;
    connDot.className   = ok ? 'conn-dot live' : 'conn-dot error';
    connText.textContent = ok ? 'Connecte - Temps reel' : 'Hors ligne - Reconnexion...';
}

// ============================================================
// UTILITAIRES
// ============================================================
function teamClass(teamName) {
    if (!teamName) return '';
    return { rouge: 'team-rouge', bleu: 'team-bleu', vert: 'team-vert', jaune: 'team-jaune' }
        [teamName.toLowerCase()] || '';
}

function teamColor(teamName) {
    return ({
        rouge: 'var(--team-red)',
        bleu:  'var(--team-blue)',
        vert:  'var(--team-green)',
        jaune: 'var(--team-yellow)'
    })[teamName?.toLowerCase()] || 'var(--text-dim)';
}

function formatDuration(ms) {
    if (!ms || ms <= 0) return '--:--';
    const s = Math.floor(ms / 1000);
    const m = Math.floor(s / 60);
    const h = Math.floor(m / 60);
    if (h > 0) return `${h}h${String(m % 60).padStart(2, '0')}`;
    return `${String(m).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
}

function formatDate(ts) {
    if (!ts) return '';
    return new Date(ts).toLocaleString('fr-FR', {
        day: '2-digit', month: '2-digit', year: 'numeric',
        hour: '2-digit', minute: '2-digit'
    });
}

function escHtml(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;')
        .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
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
    // gameState.players peut etre un objet {satId: playerObj} ou une map
    const players = Object.values(gameState.players || {});
    const alive   = players.filter(p => p.status === 'alive').length;
    const deaths  = players.reduce((a, p) => a + (p.deaths || 0), 0);

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

    // Trier : vivants en premier, puis par nombre de morts croissant
    players.sort((a, b) => {
        if (a.status !== b.status) return a.status === 'alive' ? -1 : 1;
        return (a.deaths || 0) - (b.deaths || 0);
    });

    playersGrid.innerHTML = players.map(p => {
        const isOut   = p.status === 'out';
        const tClass  = teamClass(p.team);
        const tColor  = teamColor(p.team);
        const dColor  = p.deaths === 0 ? 'var(--text-soft)'
                      : p.deaths < 3  ? 'var(--orange)'
                      :                  'var(--out-color)';
        return `
        <div class="player-card ${isOut ? 'is-out' : ''}" style="--team-color:${tColor}">
            <div class="pc-header">
                <div class="pc-name">${escHtml(p.player)}</div>
                <div class="pc-team-badge ${tClass}">${escHtml(p.team || '---')}</div>
            </div>
            <div class="pc-meta">
                <span class="pc-role">${escHtml(p.role || 'Soldat')}</span>
                <span class="pc-deaths" style="color:${dColor}">
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
    if (gameState?.status === 'running' && gameState.startTime) {
        qsTimer.textContent = formatDuration(Date.now() - gameState.startTime);
    } else if (gameState?.status === 'ended' && gameState.startTime && gameState.endTime) {
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
    } catch {
        historyList.innerHTML = '<p style="color:var(--text-dim);text-align:center">Historique indisponible (serveur hors ligne)</p>';
    }
}

function renderHistory(data) {
    if (!historyList) return;
    if (!data?.length) {
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
                        ${escHtml(p.player)} — ${p.deaths || 0} mort${(p.deaths || 0) !== 1 ? 's' : ''}
                    </span>`).join('')}
                ${(game.players?.length || 0) > 8 ? `<span class="hc-pill">+${game.players.length - 8} autres</span>` : ''}
            </div>
        </div>`;
    }).join('');
}

// ============================================================
// NAVIGATION ACTIVE
// ============================================================
function updateActiveNav() {
    const sections = document.querySelectorAll('section[id]');
    const links    = document.querySelectorAll('.nav-links a');
    const scrollY  = window.scrollY + 80;
    sections.forEach(sec => {
        if (scrollY >= sec.offsetTop && scrollY < sec.offsetTop + sec.offsetHeight) {
            links.forEach(l => l.classList.toggle('active', l.getAttribute('href') === `#${sec.id}`));
        }
    });
}

// ============================================================
// TIMERS DE FOND
// ============================================================

// Mise a jour du timer de partie chaque seconde
setInterval(() => {
    if (gameState?.status === 'running') updateTimer();
}, 1000);

// Rafraichir l'historique toutes les 30s
setInterval(loadHistory, 30000);

// Polling REST de secours si STOMP non connecte
setInterval(() => {
    if (!connected) fetchState();
}, 8000);

// ============================================================
// INIT
// ============================================================
document.addEventListener('DOMContentLoaded', () => {
    connect();
    loadHistory();
    window.addEventListener('scroll', updateActiveNav, { passive: true });
});

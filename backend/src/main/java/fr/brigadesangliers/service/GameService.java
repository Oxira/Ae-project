package fr.brigadesangliers.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import fr.brigadesangliers.model.GameEvent;
import fr.brigadesangliers.model.GameState;
import fr.brigadesangliers.model.Player;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Service metier central : traitement des evenements de jeu.
 * Maintient l'etat en memoire et pousse les mises a jour WebSocket.
 */
@Service
public class GameService {

    private static final Logger log = LoggerFactory.getLogger(GameService.class);
    private static final String HISTORY_FILE = "data/history.json";
    private static final int    MAX_HISTORY  = 50;

    @Autowired
    private SimpMessagingTemplate messagingTemplate;

    @Autowired
    private ObjectMapper objectMapper;

    private volatile GameState currentGame = new GameState();
    private final List<Map<String, Object>> history = new CopyOnWriteArrayList<>();

    public GameService() {
        loadHistory();
    }

    // ============================================================
    // TRAITEMENT DES EVENEMENTS
    // ============================================================

    public synchronized void processEvent(GameEvent event) {
        if (event == null || event.getType() == null) return;

        log.info("[EVENT] type={} sat={} player={}",
                 event.getType(), event.getSat(), event.getPlayer());

        switch (event.getType()) {

            case "player_register" -> {
                if (event.getSat() == null) break;
                Player p = new Player(
                    event.getSat(),
                    orDefault(event.getPlayer(), "Joueur " + event.getSat()),
                    orDefault(event.getTeam(), "inconnu"),
                    orDefault(event.getRole(), "Soldat")
                );
                currentGame.getPlayers().put(event.getSat(), p);
            }

            case "death" -> {
                if (event.getSat() == null) break;
                Player p = currentGame.getPlayers().get(event.getSat());
                if (p != null) {
                    int deaths = event.getDeaths() != null
                        ? event.getDeaths()
                        : p.getDeaths() + 1;
                    p.setDeaths(deaths);
                    p.setStatus("out");
                }
            }

            case "revive" -> {
                if (event.getSat() == null) break;
                Player p = currentGame.getPlayers().get(event.getSat());
                if (p != null) p.setStatus("alive");
            }

            case "game_start" -> {
                currentGame.setStatus("running");
                currentGame.setStartTime(System.currentTimeMillis());
                currentGame.setEndTime(null);
                log.info("[GAME] Partie demarree !");
            }

            case "game_end" -> {
                currentGame.setStatus("ended");
                currentGame.setEndTime(System.currentTimeMillis());
                archiveGame();
                log.info("[GAME] Partie terminee, historique sauvegarde.");
            }

            case "reset" -> {
                int nextNum = history.size() + 2;
                currentGame = new GameState();
                currentGame.setName("Partie " + nextNum);
                log.info("[GAME] Remise a zero");
            }

            default -> log.warn("[EVENT] Type inconnu : {}", event.getType());
        }

        broadcastUpdate();
    }

    // ============================================================
    // BROADCAST WEBSOCKET
    // ============================================================

    private void broadcastUpdate() {
        // Envoie l'etat a tous les abonnes /topic/game-update
        messagingTemplate.convertAndSend("/topic/game-update", buildDto());
    }

    /** DTO serialise vers les clients WebSocket */
    private Map<String, Object> buildDto() {
        Map<String, Object> dto = new LinkedHashMap<>();
        dto.put("status",    currentGame.getStatus());
        dto.put("name",      currentGame.getName());
        dto.put("startTime", currentGame.getStartTime());
        dto.put("endTime",   currentGame.getEndTime());
        // Convertir la map en objet compatible JSON (key = satelliteId)
        Map<String, Object> playersMap = new LinkedHashMap<>();
        currentGame.getPlayers().forEach((k, v) -> playersMap.put(String.valueOf(k), v));
        dto.put("players", playersMap);
        return dto;
    }

    // ============================================================
    // ACCESSEURS REST
    // ============================================================

    public GameState getCurrentGame() {
        return currentGame;
    }

    public Map<String, Object> getCurrentGameDto() {
        return buildDto();
    }

    public List<Map<String, Object>> getHistory() {
        return history;
    }

    // ============================================================
    // HISTORIQUE (persistence JSON simple)
    // ============================================================

    @SuppressWarnings("unchecked")
    private void archiveGame() {
        Map<String, Object> entry = new LinkedHashMap<>();
        entry.put("name",      currentGame.getName());
        entry.put("startTime", currentGame.getStartTime());
        entry.put("endTime",   currentGame.getEndTime());
        entry.put("duration",
            currentGame.getStartTime() != null && currentGame.getEndTime() != null
                ? currentGame.getEndTime() - currentGame.getStartTime()
                : null);
        entry.put("players", new ArrayList<>(currentGame.getPlayers().values()));

        history.add(0, entry);
        while (history.size() > MAX_HISTORY) history.remove(history.size() - 1);

        saveHistory();
    }

    private void saveHistory() {
        try {
            Files.createDirectories(Paths.get("data"));
            objectMapper.writeValue(new File(HISTORY_FILE), history);
        } catch (IOException e) {
            log.error("[HISTORY] Erreur sauvegarde : {}", e.getMessage());
        }
    }

    @SuppressWarnings("unchecked")
    private void loadHistory() {
        try {
            File f = new File(HISTORY_FILE);
            if (f.exists()) {
                List<?> loaded = objectMapper.readValue(f, List.class);
                loaded.forEach(o -> history.add((Map<String, Object>) o));
                log.info("[HISTORY] {} parties chargees.", history.size());
            }
        } catch (IOException e) {
            log.warn("[HISTORY] Impossible de charger : {}", e.getMessage());
        }
    }

    // ---- utilitaire ----
    private String orDefault(String val, String def) {
        return (val != null && !val.isBlank()) ? val : def;
    }
}

package fr.brigadesangliers.model;

import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Etat courant de la partie, diffuse aux clients WebSocket.
 */
public class GameState {

    /** idle | running | ended */
    private String status = "idle";

    private String name      = "Partie 1";
    private Long   startTime = null;
    private Long   endTime   = null;

    /** Joueurs indexes par satellite ID */
    private Map<Integer, Player> players = new LinkedHashMap<>();

    public GameState() {}

    // ---- Getters / Setters ----

    public String getStatus()              { return status; }
    public void   setStatus(String status) { this.status = status; }

    public String getName()               { return name; }
    public void   setName(String name)    { this.name = name; }

    public Long   getStartTime()               { return startTime; }
    public void   setStartTime(Long startTime) { this.startTime = startTime; }

    public Long   getEndTime()               { return endTime; }
    public void   setEndTime(Long endTime)   { this.endTime = endTime; }

    public Map<Integer, Player> getPlayers()                       { return players; }
    public void                  setPlayers(Map<Integer, Player> p) { this.players = p; }

    // Commodite : collection pour la serialisation JSON (tableau)
    public Collection<Player> getPlayerList() {
        return players.values();
    }
}

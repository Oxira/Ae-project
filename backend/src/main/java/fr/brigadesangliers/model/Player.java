package fr.brigadesangliers.model;

/**
 * Etat d'un joueur pendant une partie.
 */
public class Player {

    private int    satelliteId;
    private String player;
    private String team;
    private String role;
    private int    deaths;
    private String status;      // "alive" | "out"
    private long   joinedAt;    // timestamp ms

    public Player() {}

    public Player(int satelliteId, String player, String team, String role) {
        this.satelliteId = satelliteId;
        this.player      = player;
        this.team        = team;
        this.role        = role;
        this.deaths      = 0;
        this.status      = "alive";
        this.joinedAt    = System.currentTimeMillis();
    }

    // ---- Getters / Setters ----

    public int    getSatelliteId()               { return satelliteId; }
    public void   setSatelliteId(int satelliteId) { this.satelliteId = satelliteId; }

    public String getPlayer()                    { return player; }
    public void   setPlayer(String player)       { this.player = player; }

    public String getTeam()                      { return team; }
    public void   setTeam(String team)           { this.team = team; }

    public String getRole()                      { return role; }
    public void   setRole(String role)           { this.role = role; }

    public int    getDeaths()                    { return deaths; }
    public void   setDeaths(int deaths)          { this.deaths = deaths; }

    public String getStatus()                    { return status; }
    public void   setStatus(String status)       { this.status = status; }

    public long   getJoinedAt()                  { return joinedAt; }
    public void   setJoinedAt(long joinedAt)     { this.joinedAt = joinedAt; }
}

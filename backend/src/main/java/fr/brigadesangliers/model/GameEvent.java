package fr.brigadesangliers.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

/**
 * Evenement recu du serveur terrain via MQTT.
 * Correspond au JSON publie par le field-server apres reception LoRa.
 *
 * Types possibles :
 *   player_register, death, revive, game_start, game_end, reset
 */
@JsonIgnoreProperties(ignoreUnknown = true)
public class GameEvent {

    /** Type d'evenement */
    private String type;

    /** Index du satellite (0..N) */
    private Integer sat;

    /** Nom du joueur */
    private String player;

    /** Equipe (Rouge, Bleu, Vert, Jaune) */
    private String team;

    /** Role (Soldat, Medic, Cmd., Sniper, Scout) */
    private String role;

    /** Nombre total de morts du joueur */
    private Integer deaths;

    /** Statut : "alive" ou "out" */
    private String status;

    /** Timestamp Arduino (millis) */
    private Long ts;

    /** RSSI LoRa mesure par le serveur terrain (dBm) */
    private Integer lora_rssi;

    /** SNR LoRa mesure par le serveur terrain (dB) */
    private Double lora_snr;

    // ---- Getters / Setters ----

    public String getType()       { return type; }
    public void   setType(String type) { this.type = type; }

    public Integer getSat()       { return sat; }
    public void    setSat(Integer sat) { this.sat = sat; }

    public String getPlayer()     { return player; }
    public void   setPlayer(String player) { this.player = player; }

    public String getTeam()       { return team; }
    public void   setTeam(String team) { this.team = team; }

    public String getRole()       { return role; }
    public void   setRole(String role) { this.role = role; }

    public Integer getDeaths()    { return deaths; }
    public void    setDeaths(Integer deaths) { this.deaths = deaths; }

    public String getStatus()     { return status; }
    public void   setStatus(String status) { this.status = status; }

    public Long   getTs()         { return ts; }
    public void   setTs(Long ts)  { this.ts = ts; }

    public Integer getLoraRssi()  { return lora_rssi; }
    public void    setLoraRssi(Integer lora_rssi) { this.lora_rssi = lora_rssi; }

    public Double  getLoraSnr()   { return lora_snr; }
    public void    setLoraSnr(Double lora_snr) { this.lora_snr = lora_snr; }
}

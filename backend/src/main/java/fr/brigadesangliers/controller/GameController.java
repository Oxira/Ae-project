package fr.brigadesangliers.controller;

import fr.brigadesangliers.model.GameEvent;
import fr.brigadesangliers.service.GameService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

/**
 * API REST du backend airsoft.
 * Accessible par le frontend et optionnellement par la valise en fallback HTTP.
 */
@RestController
@RequestMapping("/api")
@CrossOrigin(origins = "*")
public class GameController {

    @Autowired
    private GameService gameService;

    // ----------------------------------------------------------------
    // GET /api/state  -- etat courant de la partie
    // ----------------------------------------------------------------
    @GetMapping("/state")
    public ResponseEntity<Map<String, Object>> getState() {
        return ResponseEntity.ok(gameService.getCurrentGameDto());
    }

    // ----------------------------------------------------------------
    // GET /api/history  -- 20 dernieres parties
    // ----------------------------------------------------------------
    @GetMapping("/history")
    public ResponseEntity<List<Map<String, Object>>> getHistory() {
        List<Map<String, Object>> h = gameService.getHistory();
        int size = Math.min(h.size(), 20);
        return ResponseEntity.ok(h.subList(0, size));
    }

    // ----------------------------------------------------------------
    // POST /api/event  -- fallback HTTP (si MQTT indisponible)
    // Meme format que le message MQTT publie par le serveur terrain
    // ----------------------------------------------------------------
    @PostMapping("/event")
    public ResponseEntity<Map<String, Object>> receiveEvent(@RequestBody GameEvent event) {
        gameService.processEvent(event);
        return ResponseEntity.ok(Map.of("ok", true, "ts", System.currentTimeMillis()));
    }

    // ----------------------------------------------------------------
    // POST /api/game/start  -- demarrage manuel (admin web)
    // ----------------------------------------------------------------
    @PostMapping("/game/start")
    public ResponseEntity<Map<String, Object>> startGame(
            @RequestBody(required = false) Map<String, String> body) {

        GameEvent event = new GameEvent();
        event.setType("game_start");
        if (body != null && body.containsKey("name")) {
            gameService.getCurrentGame().setName(body.get("name"));
        }
        gameService.processEvent(event);
        return ResponseEntity.ok(Map.of("ok", true));
    }

    // ----------------------------------------------------------------
    // POST /api/game/end  -- fin manuelle (admin web)
    // ----------------------------------------------------------------
    @PostMapping("/game/end")
    public ResponseEntity<Map<String, Object>> endGame() {
        GameEvent event = new GameEvent();
        event.setType("game_end");
        gameService.processEvent(event);
        return ResponseEntity.ok(Map.of("ok", true));
    }

    // ----------------------------------------------------------------
    // POST /api/game/reset  -- remise a zero
    // ----------------------------------------------------------------
    @PostMapping("/game/reset")
    public ResponseEntity<Map<String, Object>> resetGame() {
        GameEvent event = new GameEvent();
        event.setType("reset");
        gameService.processEvent(event);
        return ResponseEntity.ok(Map.of("ok", true));
    }
}

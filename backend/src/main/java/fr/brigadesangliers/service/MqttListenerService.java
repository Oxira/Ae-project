package fr.brigadesangliers.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import fr.brigadesangliers.model.GameEvent;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

/**
 * Abonnement au broker MQTT et reception des evenements de jeu.
 * Se reconnecte automatiquement en cas de deconnexion.
 */
@Service
public class MqttListenerService implements MqttCallbackExtended {

    private static final Logger log = LoggerFactory.getLogger(MqttListenerService.class);

    @Value("${mqtt.broker.url}")
    private String brokerUrl;

    @Value("${mqtt.client.id}")
    private String clientId;

    @Value("${mqtt.topic.events}")
    private String topicEvents;

    @Value("${mqtt.username:}")
    private String username;

    @Value("${mqtt.password:}")
    private String password;

    @Autowired
    private GameService gameService;

    @Autowired
    private ObjectMapper objectMapper;

    private MqttClient client;

    @PostConstruct
    public void init() {
        connect();
    }

    @PreDestroy
    public void shutdown() {
        try {
            if (client != null && client.isConnected()) client.disconnect();
        } catch (MqttException e) {
            log.warn("[MQTT] Erreur deconnexion propre : {}", e.getMessage());
        }
    }

    // ============================================================
    // CONNEXION
    // ============================================================

    private void connect() {
        try {
            client = new MqttClient(brokerUrl, clientId, null);
            client.setCallback(this);

            MqttConnectOptions opts = new MqttConnectOptions();
            opts.setAutomaticReconnect(true);   // Reconnexion automatique
            opts.setCleanSession(false);         // Conserver les messages en attente
            opts.setConnectionTimeout(10);
            opts.setKeepAliveInterval(30);

            if (!username.isBlank()) {
                opts.setUserName(username);
                opts.setPassword(password.toCharArray());
            }

            log.info("[MQTT] Connexion a {} ...", brokerUrl);
            client.connect(opts);

        } catch (MqttException e) {
            log.error("[MQTT] Echec connexion initiale : {}", e.getMessage());
            // PubSubClient gerera la reconnexion via automaticReconnect
        }
    }

    // ============================================================
    // CALLBACKS MQTT
    // ============================================================

    @Override
    public void connectComplete(boolean reconnect, String serverURI) {
        log.info("[MQTT] {} connecte a {}", reconnect ? "Reconnecte" : "Connecte", serverURI);
        try {
            // (Re)s'abonner apres chaque connexion
            client.subscribe(topicEvents, 1);
            log.info("[MQTT] Abonne a : {}", topicEvents);
        } catch (MqttException e) {
            log.error("[MQTT] Erreur abonnement : {}", e.getMessage());
        }
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        String payload = new String(message.getPayload());
        log.debug("[MQTT RX] topic={} payload={}", topic, payload);

        try {
            GameEvent event = objectMapper.readValue(payload, GameEvent.class);
            gameService.processEvent(event);
        } catch (Exception e) {
            log.error("[MQTT RX] Payload JSON invalide : {} | {}", payload, e.getMessage());
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        log.warn("[MQTT] Connexion perdue : {} (reconnexion automatique en cours...)",
                 cause.getMessage());
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {
        // Non utilise (pas de publication depuis le backend vers MQTT)
    }
}

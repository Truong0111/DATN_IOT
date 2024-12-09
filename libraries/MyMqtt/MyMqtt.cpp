#include "MyMqtt.h"

WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("Esp32", MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("MQTT client connected");
      client.subscribe(MQTT_TOPIC);
      Serial.println("Subscribed to: " + String(MQTT_TOPIC));
    } else {
      Serial.printf("Failed, rc=%d. Trying again in 5 seconds.\n", client.state());
      delay(5000);
    }
  }
}

void send_message_mqtt(String msg) {
  Serial.println("Send message: " + msg);
  client.publish(MQTT_TOPIC, msg.c_str());
}

void setup_mqtt(CallbackFunction callback) {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}
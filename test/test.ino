#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>

WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;

// MQTT Broker information
const char* mqtt_server = "192.168.0.118";
const int mqtt_port = 1883;
const char* mqtt_topic = "door/qr";
const char* mqtt_username = "truongtq";
const char* mqtt_password = "20204859";

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Function to handle incoming messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// Reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Sử dụng username và password
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Subscribe to a topic
      client.subscribe(mqtt_topic);
      Serial.print("Subscribed to topic: ");
      Serial.println(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);
  WiFi.mode(WIFI_STA);

  // Add list of WiFi networks
  wifiMulti.addAP("TTL", "truongtq204859");

  // Connect to Wi-Fi
  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    String macAddress = WiFi.macAddress();
    Serial.print("MAC Address: ");
    Serial.println(macAddress);
  }
  
  // Configure MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Reconnect if necessary
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(1000);
}

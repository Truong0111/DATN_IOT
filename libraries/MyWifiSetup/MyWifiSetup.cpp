#include "MyWifiSetup.h"

WiFiMulti wifiMulti;

// WiFi setup
void setup_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Add list of WiFi networks
    wifiMulti.addAP("TTL", "truongtq204859");
    wifiMulti.addAP("Scary", "truong204859");

    Serial.println("Connecting to WiFi...");
    if (wifiMulti.run() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.println("IP address: " + WiFi.localIP().toString());
        Serial.println("MAC address: " + WiFi.macAddress());
    }
}

bool is_wifi_connect(){
    return wifiMulti.run() == WL_CONNECTED;
}

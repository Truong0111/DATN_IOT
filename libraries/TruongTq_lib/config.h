#define LV_VER_RES 480
#define LV_HOR_RES 320

// Touchscreen pins
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS


// LED PIN
#define LED_GREEN_PIN 17
#define LED_BLUE_PIN 16

#define RDM6300_RX_PIN 35
#define SPEAKER_PIN 26
#define WHISTLE_PIN 22
#define RELAY_PIN 21
#define RST_PIN -1
#define SS_PIN 5

//WIFI
#define WIFI_RECONNECT_TIME 5000

//MQTT
#define MQTT_SERVER "192.168.0.118"
#define MQTT_PORT 1883
#define MQTT_TOPIC "door/qr"
#define MQTT_USERNAME "esp32"
#define MQTT_PASSWORD "esp32"

#define FROM_SERVER "SERVER"
#define FROM_ESP "ESP"

#define RFID "RFID"
#define REQUEST_ID_DOOR "REQUEST_ID_DOOR"
#define UPDATE_TOKEN "UPDATE_TOKEN"
#define UPDATE_MAC "UPDATE_MAC"
#define SET_ID_DOOR "SET_ID_DOOR"
#define ACCESS_DOOR "ACCESS_DOOR"
#define CHECK_TOKEN "CHECK_TOKEN"
#define RESPONSE_CHECK_TOKEN "RESPONSE_CHECK_TOKEN"

//Preferences
#define ACCEPT "accept"
#define REJECT "reject"
#define PREF_ID_DOOR "PREF_ID_DOOR"
#define PREF_SSID "PREF_SSID"
#define PREF_SSID_PWD "PREF_PASSWORD"
#define PREF_IP_MQTT "PREF_IP_MQTT"

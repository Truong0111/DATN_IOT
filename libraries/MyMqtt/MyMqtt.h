#ifndef MY_MQTT_H
#define MY_MQTT_H

#include <WiFi.h>
#include <TruongTq.h>
#include <PubSubClient.h>

extern WiFiClient espClient;
extern PubSubClient client;

typedef void (*CallbackFunction)(char*, uint8_t*, unsigned int);

void reconnect();
void send_message_mqtt(String msg);
void setup_mqtt(CallbackFunction callback);

#endif

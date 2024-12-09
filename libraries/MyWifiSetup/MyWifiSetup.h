#ifndef MY_WIFI_SETUP_H
#define MY_WIFI_SETUP_H

#include <WiFi.h>
#include <WiFiMulti.h>

extern WiFiMulti wifiMulti;

void setup_wifi();
bool is_wifi_connect();

#endif

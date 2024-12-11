#include <Arduino.h>

#include <TruongTq.h>
#include <MyUtil.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include <Preferences.h>
Preferences preferences;

#include <WiFi.h>
#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress ip;

void receive_bluetooth_data() {
  if (SerialBT.available()) {
    char received[100];
    int index = 0;

    while (SerialBT.available()) {
      char byteReceived = SerialBT.read();
      if (byteReceived == '\n' || index >= sizeof(received) - 1) {
        break;
      }
      received[index++] = byteReceived;
    }

    received[index] = '\0';

    Serial.println("Received: ");
    Serial.println(received);

    String data = String(received);

    int firstDelimiter = data.indexOf('|');
    int secondDelimiter = data.indexOf('|', firstDelimiter + 1);

    if (firstDelimiter == -1) {
      Serial.println("Error: Invalid format.");
      return;
    }

    String type = data.substring(0, firstDelimiter);

    if (type == "WIFI") {
      String ssid = data.substring(firstDelimiter + 1, secondDelimiter);
      String password = data.substring(secondDelimiter + 1);

      if (ssid.length() > 0 && password.length() > 0) {
        preferences.putString(PREF_SSID, ssid);
        preferences.putString(PREF_SSID_PWD, password);

        WiFi.begin(ssid.c_str(), password.c_str());
      } else {
        Serial.println("Error: Invalid WIFI format.");
      }
    } else if (type == "MQTT") {
      String serverIP = data.substring(firstDelimiter + 1);

      if (serverIP.length() > 0) {
        if (ip.fromString(serverIP)) {
          client.disconnect();
          client.setServer(ip, MQTT_PORT);
          preferences.putString(PREF_IP_MQTT, serverIP);
        }
      } else {
        Serial.println("Error: Invalid MQTT format.");
      }
    } else {
      Serial.printf("Unknown type: %s\n", type.c_str());
    }
  } else {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  String saveSSID = preferences.getString(PREF_SSID, "");
  String saveSSIDPassword = preferences.getString(PREF_SSID_PWD, "");

  Serial.println(saveSSID + " " + saveSSID.length());
  Serial.println(saveSSIDPassword + " " + saveSSIDPassword.length());

  // Add list of WiFi networks
  if (!saveSSID.isEmpty() && !saveSSIDPassword.isEmpty()) {
    WiFi.begin(saveSSID.c_str(), saveSSIDPassword.c_str());
  } else {
    WiFi.begin("TTL", "truongtq204859");
  }
}

void reconnect_mqtt() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("Esp32", MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("MQTT client connected");
      client.subscribe(MQTT_TOPIC);
      Serial.println("Subscribed to: " + String(MQTT_TOPIC));
    } else {
      Serial.printf("Failed, rc=%d. Trying again in 5 seconds.\n", client.state());
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

void send_message_mqtt(String msg) {
  Serial.println("Send message: " + msg);
  client.publish(MQTT_TOPIC, msg.c_str());
}

void setup_mqtt() {
  String mqtt_server = preferences.getString(PREF_IP_MQTT, "");
  if (mqtt_server.isEmpty()) {
    client.setServer(MQTT_SERVER, MQTT_PORT);
  } else if (ip.fromString(mqtt_server)) {
    client.setServer(ip, MQTT_PORT);
  }

  client.setCallback(callback);
}

// UI objects
lv_obj_t *qrUser = nullptr;
lv_obj_t *qrManager = nullptr;

// Global variables
String my_mac_address = "";
String my_token = "";
bool is_update_mac = false;
bool is_update_qr_user = false;

// Callback
void callback(char *topic, byte *payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String arr[5];
  int arrSize = 0;
  split_message_mqtt(message, "::", arr, arrSize);

  if (arrSize < 2 || arr[0] != FROM_SERVER)
    return;

  if (arr[1] == UPDATE_TOKEN) {
    update_qr_user(arr[2], arr[3], arr[4]);
  } else if (arr[1] == SET_ID_DOOR) {
    save_id_door(arr[2], arr[3]);
  } else if (arr[1] == ACCESS_DOOR) {
    access_door(arr[2], arr[3]);
  } else if (arr[1] == RESPONSE_CHECK_TOKEN) {
    get_response_check_token(arr[2], arr[3], arr[4]);
  }
}

void save_id_door(String macAddress, String idDoor) {
  if (my_mac_address != macAddress) return;
  preferences.putString(PREF_ID_DOOR, idDoor);
}

// LVGL
#include <XPT2046_Touchscreen.h>

#define DRAW_BUF_SIZE (LV_HOR_RES * LV_VER_RES / 20 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

int x, y, z;
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = LV_HOR_RES - map(p.x, 200, 3700, 1, LV_HOR_RES);
    y = map(p.y, 240, 3800, 1, LV_VER_RES);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void log_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void setup_lvgl() {
  lv_init();
  lv_log_register_print_cb(log_print);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  lv_disp_t *disp;
  disp = lv_tft_espi_create(LV_HOR_RES, LV_VER_RES, draw_buf, sizeof(draw_buf));
  lv_disp_set_rotation(disp, LV_DISP_ROTATION_270);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  lv_create_tab_gui();
}

void lv_create_tab_gui(void) {
  lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);

  lv_obj_t *tabUser = lv_tabview_add_tab(tabview, "User");
  lv_obj_t *tabManager = lv_tabview_add_tab(tabview, "Manager");

  // Qr color
  lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
  lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

  // User tab
  // Qr container
  qrUser = lv_qrcode_create(tabUser);
  lv_qrcode_set_size(qrUser, 200);
  lv_qrcode_set_dark_color(qrUser, fg_color);
  lv_qrcode_set_light_color(qrUser, bg_color);
  lv_obj_center(qrUser);

  // QR border
  lv_obj_set_style_border_color(qrUser, bg_color, 0);
  lv_obj_set_style_border_width(qrUser, 5, 0);

  // Manger tab
  // Qr container
  qrManager = lv_qrcode_create(tabManager);
  lv_qrcode_set_size(qrManager, 200);
  lv_qrcode_set_dark_color(qrManager, fg_color);
  lv_qrcode_set_light_color(qrManager, bg_color);
  lv_obj_center(qrManager);

  // QR border
  lv_obj_set_style_border_color(qrManager, bg_color, 0);
  lv_obj_set_style_border_width(qrManager, 5, 0);
}

void update_qr_user(String macAddress, String idDoor, String token) {
  if (my_mac_address != macAddress) return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor) return;
  my_token = idDoor + "::" + token;
  const char *data = my_token.c_str();
  lv_qrcode_update(qrUser, data, strlen(data));
}

void update_qr_manager() {
  String current_mac = WiFi.macAddress();
  if (my_mac_address != current_mac) {
    my_mac_address = current_mac;
    lv_qrcode_update(qrManager, my_mac_address.c_str(), my_mac_address.length());
    is_update_mac = false;
  }

  if (is_update_mac) return;
  if (!client.connected()) return;

  String door_id = preferences.getString(PREF_ID_DOOR, "");
  if (door_id.isEmpty()) return;
  String sendMsg = String(FROM_ESP) + "::" + String(UPDATE_MAC) + "::" + my_mac_address + "::" + door_id;
  send_message_mqtt(sendMsg);
  is_update_mac = true;
}

void access_door(String macAddress, String idDoor) {
  if (my_mac_address != macAddress) return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor) return;
  ledcWrite(LED_GREEN_PIN, 0);
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  ledcWrite(LED_GREEN_PIN, 255);
}

void check_token() {
  if (!client.connected()) return;
  if (is_update_qr_user) return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor.isEmpty()) return;
  String requestQrUserMsg = String(FROM_ESP) + "::" + String(CHECK_TOKEN) + "::" + my_mac_address + "::" + myIdDoor;
  send_message_mqtt(requestQrUserMsg);
  is_update_qr_user = true;
  vTaskDelay(2000 / portTICK_PERIOD_MS);
}

void get_response_check_token(String macAddress, String idDoor, String response) {
  if (my_mac_address != macAddress) return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor) return;
  if (response == "TokenInvalid") {
    is_update_qr_user = false;
  } else {
    update_qr_user(macAddress, idDoor, response);
  }
}

void setup() {
  Serial.begin(115200);

  preferences.begin("my-esp", false);

  SerialBT.begin("ESP32_LVGL");
  delay(20);

  // Led init
  pinMode(LED_GREEN_PIN, OUTPUT);
  ledcAttach(LED_GREEN_PIN, 5000, 8);
  ledcWrite(LED_GREEN_PIN, 255);

  // Wifi init
  setup_wifi();
  delay(20);
  // MQTT init
  setup_mqtt();
  delay(20);
  // LVGL init
  setup_lvgl();
  delay(20);
}

void main_loop() {
  if (SerialBT.isReady()) {
    receive_bluetooth_data();
  }

  if (WiFi.status() != WL_CONNECTED) return;

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  update_qr_manager();
  if (my_token.isEmpty())
    check_token();
}

void loop() {
  lv_task_handler();
  lv_tick_inc(5);
  delay(10);
  main_loop();
}
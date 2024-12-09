#include <Arduino.h>

#include <TruongTq.h>
#include <MyUtil.h>
#include <MyWifiSetup.h>
#include <MyMqtt.h>

#include <WiFi.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include <Preferences.h>
Preferences preferences;

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

#define DRAW_BUF_SIZE (LV_HOR_RES * LV_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
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
  if (!is_wifi_connect()) return;

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
  delay(10000);
  ledcWrite(LED_GREEN_PIN, 255);
}

void check_token() {
  if (!is_wifi_connect()) return;
  if (!client.connected()) return;
  if (is_update_qr_user) return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor.isEmpty()) return;
  String requestQrUserMsg = String(FROM_ESP) + "::" + String(CHECK_TOKEN) + "::" + my_mac_address + "::" + myIdDoor;
  send_message_mqtt(requestQrUserMsg);
  is_update_qr_user = true;
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

  // Led init
  pinMode(LED_GREEN_PIN, OUTPUT);
  ledcAttach(LED_GREEN_PIN, 5000, 8);
  ledcWrite(LED_GREEN_PIN, 255);

  // Wifi init
  setup_wifi();
  delay(20);
  // MQTT init
  setup_mqtt(callback);
  delay(20);
  // LVGL init
  setup_lvgl();
  delay(20);

  preferences.begin("my-esp", false);
}

void main_loop() {
  if (!is_wifi_connect()) {
    Serial.println("WiFi disconnected. Reconnecting...");
    wifiMulti.run();
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  update_qr_manager();
  if (!my_token.isEmpty()) return;
  check_token();
}

void loop() {
  lv_task_handler();
  lv_tick_inc(5);
  delay(10);
  main_loop();
}

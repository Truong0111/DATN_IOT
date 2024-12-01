#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#define LV_VER_RES 480
#define LV_HOR_RES 320

#define DRAW_BUF_SIZE (LV_HOR_RES * LV_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

#include <XPT2046_Touchscreen.h>
// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

//Wifi scan list max
#define WL_NETWORKS_LIST_MAXNUM 20

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

void log_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

int x, y, z;

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

lv_obj_t *wifiPopup = NULL;
lv_obj_t *passwordField = NULL;
lv_obj_t *keyboard = NULL;
String selectedSSID = "";

void disconnect_wifi() {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  Serial.println("Disconnect wifi");
  if (WiFi.isConnected()) {
    WiFi.disconnect();
    Serial.println("Disconnected from WiFi");
  }
}

void hide_wifi_popup() {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  Serial.println("Hide wifi popup");
  if (wifiPopup != NULL) {
    lv_obj_del(wifiPopup);
    wifiPopup = NULL;
  }
}

void connect_wifi() {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  Serial.printf("Connect to wifi %s:\n", selectedSSID.c_str());
  const char *password = lv_textarea_get_text(passwordField);
  WiFi.begin(selectedSSID.c_str(), password);
  if (WiFi.isConnected()) {
    Serial.printf("Connecting to WiFi: %s successfull!\n", selectedSSID.c_str());
  }
}

void keyboard_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (passwordField != NULL && code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(keyboard, passwordField);
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_keyboard_set_textarea(keyboard, NULL);
    // lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

void wifiButtonEventHandler(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  const char *wifiName = lv_list_get_btn_text(lv_obj_get_parent(btn), btn);

  if (wifiPopup != NULL) {
    lv_obj_del(wifiPopup);
  }

  selectedSSID = wifiName;

  wifiPopup = lv_obj_create(lv_scr_act());
  lv_obj_set_size(wifiPopup, LV_HOR_RES - 40, LV_VER_RES - 160);
  lv_obj_align(wifiPopup, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(wifiPopup, 10, 0);
  lv_obj_set_style_bg_color(wifiPopup, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(wifiPopup, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(wifiPopup, 2, 0);

  passwordField = lv_textarea_create(wifiPopup);
  lv_textarea_set_placeholder_text(passwordField, "Enter password");
  lv_obj_set_size(passwordField, LV_HOR_RES - 60, 40);
  lv_obj_align(passwordField, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_add_event_cb(passwordField, keyboard_event_handler, LV_EVENT_ALL, keyboard);

  lv_obj_t *btnConnect = lv_btn_create(wifiPopup);
  lv_obj_set_size(btnConnect, LV_HOR_RES - 60, 40);
  lv_obj_align(btnConnect, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_t *labelConnect = lv_label_create(btnConnect);
  lv_label_set_text(labelConnect, "Connect");

  lv_obj_t *btnDisconnect = lv_btn_create(wifiPopup);
  lv_obj_set_size(btnDisconnect, LV_HOR_RES - 60, 40);
  lv_obj_align(btnDisconnect, LV_ALIGN_TOP_MID, 0, 110);
  lv_obj_t *labelDisconnect = lv_label_create(btnDisconnect);
  lv_label_set_text(labelDisconnect, "Disconnect");

  lv_obj_t *btnCancel = lv_btn_create(wifiPopup);
  lv_obj_set_size(btnCancel, LV_HOR_RES - 60, 40);
  lv_obj_align(btnCancel, LV_ALIGN_TOP_MID, 0, 160);
  lv_obj_t *labelCancel = lv_label_create(btnCancel);
  lv_label_set_text(labelCancel, "Cancel");

  lv_obj_add_event_cb(
    btnConnect, [](lv_event_t *e) {
      connect_wifi();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_add_event_cb(
    btnDisconnect, [](lv_event_t *e) {
      disconnect_wifi();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_add_event_cb(
    btnCancel, [](lv_event_t *e) {
      hide_wifi_popup();
    },
    LV_EVENT_CLICKED, NULL);
}

void lv_create_tab_gui(void) {
  lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);

  lv_obj_t *tabWifi = lv_tabview_add_tab(tabview, "Wifi");
  lv_obj_t *tabUser = lv_tabview_add_tab(tabview, "User");
  lv_obj_t *tabManager = lv_tabview_add_tab(tabview, "Manager");

  // Wifi tab
  int n = WiFi.scanNetworks();

  lv_obj_t *wifiList = lv_list_create(tabWifi);
  lv_obj_set_size(wifiList, LV_HOR_RES, LV_VER_RES - 40);

  for (int i = 0; i < n; i++) {
    lv_obj_t *wifiBtn = lv_list_add_btn(wifiList, NULL, WiFi.SSID(i).c_str());
    lv_obj_add_event_cb(wifiBtn, wifiButtonEventHandler, LV_EVENT_CLICKED, NULL);
  }

  // User tab
  lv_obj_t *label = lv_label_create(tabUser);
  lv_label_set_text(label, "User tab");

  // Manager tab
  label = lv_label_create(tabManager);
  lv_label_set_text(label, "Manager tab");

  lv_obj_scroll_to_view_recursive(label, LV_ANIM_ON);

  keyboard = lv_keyboard_create(lv_scr_act());
  lv_obj_set_size(keyboard, LV_VER_RES, LV_HOR_RES / 2);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  // lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

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

void loop() {
  lv_task_handler();
  lv_tick_inc(5);
  delay(5);
}

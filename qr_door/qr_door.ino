#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>

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

//Wifi setup
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 10000;

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

lv_obj_t *qrUser = NULL;
lv_obj_t *qrManager = NULL;
String macAddress = "";

void lv_create_tab_gui(void) {
  lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);

  lv_obj_t *tabUser = lv_tabview_add_tab(tabview, "User");
  lv_obj_t *tabManager = lv_tabview_add_tab(tabview, "Manager");

  // Qr color
  lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
  lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

  // User tab
  //Qr container
  qrUser = lv_qrcode_create(tabUser);
  lv_qrcode_set_size(qrUser, 200);
  lv_qrcode_set_dark_color(qrUser, fg_color);
  lv_qrcode_set_light_color(qrUser, bg_color);
  lv_obj_center(qrUser);

  //QR border
  lv_obj_set_style_border_color(qrUser, bg_color, 0);
  lv_obj_set_style_border_width(qrUser, 5, 0);

  //Manger tab
  //Qr container
  qrManager = lv_qrcode_create(tabManager);
  lv_qrcode_set_size(qrManager, 200);
  lv_qrcode_set_dark_color(qrManager, fg_color);
  lv_qrcode_set_light_color(qrManager, bg_color);
  lv_obj_center(qrManager);

  //QR border
  lv_obj_set_style_border_color(qrManager, bg_color, 0);
  lv_obj_set_style_border_width(qrManager, 5, 0);
}


unsigned long previousMillis = 0;
const long interval = 5000;

void update_qr_handler() {
  //User qr
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    String randomString = generateRandomString(10);
    const char *data = randomString.c_str();
    lv_qrcode_update(qrUser, data, strlen(data));
  }

  //Manager qr
  if (!WiFi.isConnected()) return;
  if (macAddress == WiFi.macAddress()) return;
  macAddress = WiFi.macAddress();
  const char *data = macAddress.c_str();
  lv_qrcode_update(qrManager, data, strlen(data));
}

const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()_+=-,./;'";

String generateRandomString(int length) {
  String randomString = "";
  for (int i = 0; i < length; i++) {
    randomString += charset[random(0, strlen(charset))];
  }
  return randomString;
}

void setup() {
  Serial.begin(115200);
  delay(20);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Add list of WiFi networks
  wifiMulti.addAP("TTL", "truongtq204859");
  wifiMulti.addAP("Scary", "truong204859");

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

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
  update_qr_handler();
  delay(20);
}

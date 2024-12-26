#include <Arduino.h>
#include <TruongTq.h>
#include "Adafruit_GFX.h"
#include <Adafruit_ILI9341.h>
#include <MFRC522.h>
#include <SPI.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS    15
#define TFT_RST   -1
#define TFT_DC    2

#define RDM6300_RX_PIN 22
#define READ_LED_PIN 16
#define RST_PIN -1  // Chân RST
#define SS_PIN 5

#define LV_HOR_RES 320
#define LV_VER_RES 480

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
int x, y, z;
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
MFRC522 rfid(SS_PIN, RST_PIN);

#define DRAW_BUF_SIZE (LV_HOR_RES * LV_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void log_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void lv_create_tab_gui(void) {
  lv_obj_t *label;

  lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);

  lv_obj_t *tabUser = lv_tabview_add_tab(tabview, "User");
  lv_obj_t *tabManager = lv_tabview_add_tab(tabview, "Manager");

  lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
  lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

  lv_obj_t *qrUser = lv_qrcode_create(tabUser);
  lv_qrcode_set_size(qrUser, 200);
  lv_qrcode_set_dark_color(qrUser, fg_color);
  lv_qrcode_set_light_color(qrUser, bg_color);
  lv_obj_center(qrUser);

  lv_obj_set_style_border_color(qrUser, bg_color, 0);
  lv_obj_set_style_border_width(qrUser, 5, 0);

  lv_obj_t *qrManager = lv_qrcode_create(tabManager);
  lv_qrcode_set_size(qrManager, 200);
  lv_qrcode_set_dark_color(qrManager, fg_color);
  lv_qrcode_set_light_color(qrManager, bg_color);
  lv_obj_center(qrManager);

  lv_obj_set_style_border_color(qrManager, bg_color, 0);
  lv_obj_set_style_border_width(qrManager, 5, 0);
}

static void touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();

    Serial.print("Point x = ");
    Serial.print(p.x);
    Serial.print(" - Point y = ");
    Serial.print(p.y);
    Serial.println();

    x = LV_HOR_RES - map(p.x, 200, 3700, 1, LV_HOR_RES);
    y = map(p.y, 240, 3800, 1, LV_VER_RES);
    z = p.z;

    Serial.print("Mapped Point x = ");
    Serial.print(x);
    Serial.print(" - Mapped Point y = ");
    Serial.print(y);
    Serial.println();

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void my_disp_flush(lv_disp_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColor(&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}
void setup_lvgl() {
  lv_init();
  lv_log_register_print_cb(log_print);

  lv_disp_t *disp;
  disp = lv_display_create(LV_HOR_RES, LV_VER_RES);
  // disp = lv_tft_espi_create(LV_HOR_RES, LV_VER_RES, draw_buf, sizeof(draw_buf));
  lv_disp_set_rotation(disp, LV_DISP_ROTATION_270);

  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read);

  lv_create_tab_gui();
}

#define RFID_MOSI 23  //MOSI
#define RFID_MISO 19  //MISO
#define RFID_SCK 18   //SCK
#define RFID_SDA 5    //CS


void setup() {
  Serial.begin(115200);

  pinMode(READ_LED_PIN, OUTPUT);
  digitalWrite(READ_LED_PIN, HIGH);
  SPI.begin();

  ts.begin();
  ts.setRotation(0);

  rfid.PCD_Init();

  setup_lvgl();
}


void loop() {
  lv_task_handler();
  lv_tick_inc(10);
  delay(10);

  if (rfid.PICC_IsNewCardPresent()) {
    Serial.println("New card detected!");
    // Sau khi phát hiện thẻ mới, có thể tiếp tục đọc serial ID của thẻ
    if (rfid.PICC_ReadCardSerial()) {
      Serial.print("Card UID: ");
      char uid_str[3 * rfid.uid.size + 1];
      for (size_t i = 0; i < rfid.uid.size; i++) {
        snprintf(uid_str + i * 2, 3, "%02X", rfid.uid.uidByte[i]);
      }
      Serial.println(uid_str);
    }
    // Hủy thẻ hiện tại sau khi đọc
    rfid.PICC_HaltA();
  }
}

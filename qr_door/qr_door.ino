#include <Arduino.h>
#include <TruongTq.h>
#include <MyUtil.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <string>
#include <sstream>
#include <iomanip>

#include <Preferences.h>
Preferences preferences;

#include <WiFi.h>
#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress ip;

#include <SPIFFS.h>
#include <rdm6300.h>
#include <MFRC522.h>
#include <SPI.h>

#define RDM6300_RX_PIN 22
#define RST_PIN -1  // Chân RST
#define SS_PIN 5    // Chân SDA/SS
#define UID_FILE "/uid_list.txt"

#define RFID_MOSI 18  //MOSI
#define RFID_MISO 19  //MISO
#define RFID_SCK 23   //SCK
#define RFID_SDA 5    //SS

MFRC522 rfid(SS_PIN, RST_PIN);
Rdm6300 rdm6300;

// UI objects
lv_obj_t *qrUser = nullptr;
lv_obj_t *qrManager = nullptr;
lv_obj_t *rfidLabel = nullptr;

// Global variables
String my_mac_address = "";
String my_token = "";
bool is_update_mac = false;
bool is_update_qr_user = false;
bool is_registering_card = false;

// RFID
void setup_rfid() {
  rdm6300.begin(RDM6300_RX_PIN);

  SPI.begin(RFID_MOSI, RFID_MISO, RFID_SCK, RFID_SDA);
  rfid.PCD_Init();

  if (!SPIFFS.begin(true)) {
    Serial.println("Can't init SPIFFS.");
    return;
  }
  Serial.println("SPIFFS ready.");
}

void rfid_rdm_loop() {
  if (rdm6300.get_new_tag_id()) {
    uint32_t uid_scan = rdm6300.get_tag_id();
    if (is_registering_card) {
      char uid_str[4 * sizeof(uid_scan)];
      snprintf(uid_str, sizeof(str), "%u", number);
      if (verify_id_card(uid_str)) {
        Serial.println("Card is already registered.");
      } else {
        save_id_card(uid_str);
        is_registering_card = false;
      }
    } else {
      char uid_str[4 * sizeof(uid_scan)];
      snprintf(uid_str, sizeof(str), "%u", number);
      if (verify_id_card(uid_str)) {
        open_door();
        vTaskDelay(1500);
      } else {
        Serial.println("Card is not registered.");
      }
    }
  }
}

template<typename TInputIter>
std::string make_hex_string(TInputIter first, TInputIter last, bool use_uppercase = true, bool insert_spaces = false) {
  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  if (use_uppercase)
    ss << std::uppercase;
  while (first != last) {
    ss << std::setw(2) << static_cast<int>(*first++);
    if (insert_spaces && first != last)
      ss << " ";
  }
  return ss.str();
}

void rfid_rc_loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  std::vector<uint8_t> uid_vector(rfid.uid.uidByte, rfid.uid.uidByte + rfid.uid.size);
  auto uid_str = make_hex_string(uid_vector.begin(), uid_vector.end(), true, false);

  if (is_registering_card) {
    if (verify_id_card(String(uid_hex.c_str()))) {
      Serial.println("Card is already registered.");
    } else {
      save_id_card(String(uid_hex.c_str()));
      is_registering_card = false;
    }
  } else {
    if (verify_id_card(String(uid_hex.c_str()))) {
      open_door();
      vTaskDelay(1500);
    } else {
      Serial.println("Card is not registered.");
    }
  }

  rfid.PICC_HaltA();
}

void save_id_card(String uid) {
  File file = SPIFFS.open(UID_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("Can't open file to write uid.");
    return;
  }

  file.println(uid);
  file.close();
  Serial.print("Đã lưu UID: ");
  Serial.println(uid);
}

bool verify_id_card(String uid) {
  std::vector<String> uidList = read_id_cards();

  for (String savedUID : uidList) {
    if (savedUID == uid) {
      return true;
    }
  }
  return false;
}

std::vector<String> read_id_cards() {
  std::vector<String> uidList;

  File file = SPIFFS.open(UID_FILE, FILE_READ);
  if (!file) {
    Serial.println("Can't open file to write uid.");
    return uidList;
  }
  while (file.available()) {
    String uid = file.readStringUntil('\n');
    uid.trim();
    if (uid.length() > 0) {
      uidList.push_back(uid);
    }
  }

  file.close();
  return uidList;
}

// Receive bluetooth data from mobile
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

      if (ssid.length() > 0) {
        preferences.putString(PREF_SSID, ssid);
        preferences.putString(PREF_SSID_PWD, password);

        if (password.length() == 0) {
          password = "";
        }

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
    } else if (type == "RFID") {
      String action = data.substring(firstDelimiter + 1, secondDelimiter);
      if (action.length() > 0) {
        if (action == "REGISTER") {
          is_registering_card = true;
        } else if (action == "REMOVE") {
          String cardUID = data.substring(secondDelimiter + 1);
        }
      }
    } else {
      Serial.printf("Unknown type: %s\n", type.c_str());
    }
  } else {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Setup wifi
void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  String saveSSID = preferences.getString(PREF_SSID, "");
  String saveSSIDPassword = preferences.getString(PREF_SSID_PWD, "");

  // Add list of WiFi networks
  if (!saveSSID.isEmpty() && !saveSSIDPassword.isEmpty()) {
    WiFi.begin(saveSSID.c_str(), saveSSIDPassword.c_str());
  } else {
    WiFi.begin("TTL", "truongtq204859");
  }
}

// Setup mqtt
void setup_mqtt() {
  String mqtt_server = preferences.getString(PREF_IP_MQTT, "");
  if (mqtt_server.isEmpty()) {
    client.setServer(MQTT_SERVER, MQTT_PORT);
  } else if (ip.fromString(mqtt_server)) {
    client.setServer(ip, MQTT_PORT);
  }

  client.setCallback(callback);
}

// Reconnect mqtt
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

// Send message to mqtt
void send_message_mqtt(String msg) {
  Serial.println("Send message: " + msg);
  client.publish(MQTT_TOPIC, msg.c_str());
}

// Get callback from mqtt
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
    // arr[2] => mac address, arr[3] => idDoor, arr[4] => token
    update_qr_user(arr[2], arr[3], arr[4]);
  } else if (arr[1] == SET_ID_DOOR) {
    // arr[2] => mac address, arr[3] => idDoor
    save_id_door(arr[2], arr[3]);
  } else if (arr[1] == ACCESS_DOOR) {
    // arr[2] => mac address, arr[3] => idDoor
    access_door(arr[2], arr[3]);
  } else if (arr[1] == RESPONSE_CHECK_TOKEN) {
    // arr[2] => mac address, arr[3] => idDoor, arr[4] => response check token from server
    get_response_check_token(arr[2], arr[3], arr[4]);
  }
}

void save_id_door(String macAddress, String idDoor) {
  if (my_mac_address != macAddress)
    return;
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
  lv_obj_t *label;

  lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);

  lv_obj_t *tabUser = lv_tabview_add_tab(tabview, "User");
  lv_obj_t *tabManager = lv_tabview_add_tab(tabview, "Manager");
  lv_obj_t *tabRfid = lv_tabview_add_tab(tabview, "RFID");

  lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
  lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

  qrUser = lv_qrcode_create(tabUser);
  lv_qrcode_set_size(qrUser, 200);
  lv_qrcode_set_dark_color(qrUser, fg_color);
  lv_qrcode_set_light_color(qrUser, bg_color);
  lv_obj_center(qrUser);

  lv_obj_set_style_border_color(qrUser, bg_color, 0);
  lv_obj_set_style_border_width(qrUser, 5, 0);

  qrManager = lv_qrcode_create(tabManager);
  lv_qrcode_set_size(qrManager, 200);
  lv_qrcode_set_dark_color(qrManager, fg_color);
  lv_qrcode_set_light_color(qrManager, bg_color);
  lv_obj_center(qrManager);

  lv_obj_set_style_border_color(qrManager, bg_color, 0);
  lv_obj_set_style_border_width(qrManager, 5, 0);

  lv_obj_t *registerCardBtn = lv_button_create(tabRfid);
  lv_obj_add_event_cb(registerCardBtn, register_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(registerCardBtn, LV_ALIGN_CENTER, 0, -40);
  lv_obj_remove_flag(registerCardBtn, LV_OBJ_FLAG_PRESS_LOCK);
}

void update_qr_user(String macAddress, String idDoor, String token) {
  if (my_mac_address != macAddress)
    return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor)
    return;
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

  if (is_update_mac)
    return;
  if (!client.connected())
    return;

  String door_id = preferences.getString(PREF_ID_DOOR, "");
  if (door_id.isEmpty())
    return;
  String sendMsg = String(FROM_ESP) + "::" + String(UPDATE_MAC) + "::" + my_mac_address + "::" + door_id;
  send_message_mqtt(sendMsg);
  is_update_mac = true;
}

void access_door(String macAddress, String idDoor) {
  if (my_mac_address != macAddress)
    return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor)
    return;
  open_door();
}

void open_door() {
  ledcWrite(LED_GREEN_PIN, 0);
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  ledcWrite(LED_GREEN_PIN, 255);
}

void check_token() {
  if (!client.connected())
    return;
  if (is_update_qr_user)
    return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor.isEmpty())
    return;
  String requestQrUserMsg = String(FROM_ESP) + "::" + String(CHECK_TOKEN) + "::" + my_mac_address + "::" + myIdDoor;
  send_message_mqtt(requestQrUserMsg);
  is_update_qr_user = true;
  vTaskDelay(2000 / portTICK_PERIOD_MS);
}

void get_response_check_token(String macAddress, String idDoor, String response) {
  if (my_mac_address != macAddress)
    return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor)
    return;
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
  delay(10);

  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_BLUE_PIN, HIGH);

  setup_rfid();
  delay(10);

  setup_wifi();
  delay(10);

  setup_mqtt();
  delay(10);

  setup_lvgl();
  delay(10);
}

void main_loop() {
  if (SerialBT.isReady()) {
    receive_bluetooth_data();
  }

  if (WiFi.status() != WL_CONNECTED) {
  }

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
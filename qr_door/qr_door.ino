#include <Arduino.h>
#include <truong_tq.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <PubSubClient.h>
#include <rdm6300.h>
#include <MFRC522.h>
#include <SPI.h>

// Define for lvgl
#define DRAW_BUF_SIZE (LV_HOR_RES * LV_VER_RES / 20 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

Preferences preferences;
BluetoothSerial SerialBT;
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress ip;
Rdm6300 rdm6300;
MFRC522 rfid(SS_PIN, RST_PIN);
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ### Kết nối RDM6300
// | Chân RDM6300 | Chân ESP32 |
// |--------------|------------|
// | TX           | GPIO35     |
// | 0V           | GND        |
// | 5V           | 5V         |

// ### Kết nối Relay
// | Chân Relay   | Chân ESP32 |
// |--------------|------------|
// | GND          | GND        |
// | 5V           | 5V         |
// | IN           | GPIO21     |

// ### Kết nối RC522
// | Chân RC522   | Chân ESP32 |
// |--------------|------------|
// | MOSI         | GPIO23     |
// | MISO         | GPIO19     |
// | SCK          | GPIO18     |
// | SS/SDA       | GPIO5      |
// | GND          | GND        |
// | RST          | 3.3V       |
// | 3.3V         | 3.3V       |

// ### Kết nối Còi
// | Chân còi     | Chân ESP32 |
// |--------------|------------|
// | +            | GPIO22     |
// | GND          | GND        |

// ### Các kết nối khác
// | Mô-đun      | Chân ESP32 |
// |-------------|------------|
// | LED Green   | GPIO17     |
// | LED Blue    | GPIO16     |


// UI objects
lv_obj_t *qrUser = nullptr;
lv_obj_t *qrManager = nullptr;
lv_obj_t *rfidLabel = nullptr;
lv_obj_t *registerCardTxt = nullptr;

// Global variables
String my_mac_address = "";
String my_token = "";
bool is_update_mac = false;
bool is_update_qr_user = false;
bool is_registering_card = false;
bool is_remove_card = false;
bool is_accept = false;
bool is_deny = false;
bool is_reading_card = false;

//---------- RFID Functions ----------
void notify_rfid_to_mqtt(const char *action, const char *uid) {
  if (WiFi.status() == WL_CONNECTED) {
    String msg = String(FROM_ESP) + "::" + String(RFID) + "::" + String(action) + "::" + my_mac_address + "::" + String(uid);
    send_message_mqtt(msg);
  }
}

void notify_access_door_to_mqtt(const char *type, bool isAccess, const char *uid, const char *status = "") {
  if (WiFi.status() == WL_CONNECTED) {
    String msg = String(FROM_ESP) + "::" + String(ACCESS_DOOR) + "::" + String(type) + "::" + String(isAccess) + "::" + String(uid) + "::" + String(status);
    send_message_mqtt(msg);
  }
}

void handle_rfid(const char *uid_str) {
  Serial.println(uid_str);
  is_reading_card = true;

  if (is_registering_card || is_remove_card) {
    bool isRegistered = verify_id_card(uid_str);

    if (is_registering_card) {
      if (isRegistered) {
        Serial.println("Card is already registered.");
        is_deny = true;
      } else {
        save_id_card(uid_str);
        is_accept = true;
        Serial.println("Card registered successfully.");
        notify_rfid_to_mqtt("REGISTER", uid_str);
      }
      is_registering_card = false;
    } else if (is_remove_card) {
      if (isRegistered) {
        remove_id_card(uid_str);
        is_accept = true;
        Serial.println("Card removed successfully.");
        notify_rfid_to_mqtt("REMOVE", uid_str);
      } else {
        Serial.println("Card is not registered.");
        is_deny = true;
      }
      is_remove_card = false;
    }

    digitalWrite(LED_BLUE_PIN, HIGH);
    hide_register_card_text();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  } else {
    if (verify_id_card(uid_str)) {
      Serial.println("Open door by card.");
      open_door();
      String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
      if (myIdDoor.isEmpty()) {
        notify_access_door_to_mqtt("RFID", true, uid_str, "No_id_door");
      } else {
        notify_access_door_to_mqtt("RFID", true, uid_str, myIdDoor.c_str());
      }
    } else {
      Serial.println("Card is not registered.");
      is_deny = true;
      notify_access_door_to_mqtt("RFID", false, uid_str, "Card_is_not_registered");
    }
  }
}

void scan_rfid() {
  if (is_reading_card) return;

  char uid_str[9];
  if (rdm6300.get_new_tag_id()) {
    snprintf(uid_str, sizeof(uid_str), "%08X", rdm6300.get_tag_id());
    handle_rfid(uid_str);
    return;
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    for (size_t i = 0; i < rfid.uid.size; i++) {
      snprintf(uid_str + i * 2, 3, "%02X", rfid.uid.uidByte[i]);
    }

    handle_rfid(uid_str);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

void save_id_card(const char *uid) {
  preferences.putString(uid, String(ACCEPT));
  indicate_action("Save UID: ", uid);
}

void remove_id_card(const char *uid) {
  preferences.putString(uid, String(REJECT));
  indicate_action("Remove UID: ", uid);
}

bool verify_id_card(const char *uid) {
  return preferences.getString(uid, String(REJECT)) == String(ACCEPT);
}

void indicate_action(const char *action, const char *uid) {
  Serial.print(action);
  Serial.println(uid);
  digitalWrite(LED_GREEN_PIN, LOW);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  digitalWrite(LED_GREEN_PIN, HIGH);
}

//----------------------------------

//---------- Bluetooth Functions ----------
//  Receive bluetooth data from mobile
void receive_bluetooth_data() {
  if (!SerialBT.available()) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    return;
  }

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
  String param1 = data.substring(firstDelimiter + 1, secondDelimiter);
  String param2 = data.substring(secondDelimiter + 1);

  if (type == "WIFI") {
    handle_wifi_config(param1, param2);
  } else if (type == "MQTT") {
    handle_mqtt_config(param1);
  } else if (type == "RFID") {
    handle_rfid_action(param1);
  } else {
    Serial.printf("Unknown type: %s\n", type.c_str());
  }
}

void handle_wifi_config(const String &ssid, const String &password) {
  if (ssid.isEmpty()) {
    Serial.println("Error: Invalid WIFI format.");
    return;
  }

  preferences.putString(PREF_SSID, ssid);
  preferences.putString(PREF_SSID_PWD, password);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid.c_str(), password.isEmpty() ? nullptr : password.c_str());
}

void handle_mqtt_config(const String &serverIP) {
  if (serverIP.isEmpty() || !ip.fromString(serverIP)) {
    Serial.println("Error: Invalid MQTT format.");
    return;
  }

  client.disconnect();
  client.setServer(ip, MQTT_PORT);
  preferences.putString(PREF_IP_MQTT, serverIP);
  Serial.println("MQTT server updated.");
}

void handle_rfid_action(const String &action) {
  if (action == "REGISTER") {
    is_registering_card = true;
    digitalWrite(LED_BLUE_PIN, LOW);
    show_register_card_text(true);
  } else if (action == "REMOVE") {
    is_remove_card = true;
    show_register_card_text(false);
  } else if (action == "CANCEL") {
    is_registering_card = false;
    is_remove_card = false;
    digitalWrite(LED_BLUE_PIN, HIGH);
    hide_register_card_text();
  } else {
    Serial.printf("Unknown RFID action: %s\n", action.c_str());
  }
}

void handle_wifi_reconnect() {
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  if (now - lastReconnectAttempt > WIFI_RECONNECT_TIME) {
    lastReconnectAttempt = now;
    Serial.println("Reconnecting to WiFi...");
    WiFi.reconnect();
  }
}

//----------------------------------

//---------- MQTT Functions ----------
void reconnect_mqtt() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("Esp32", MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("MQTT client connected");
      client.setKeepAlive(30);
      client.subscribe(MQTT_TOPIC);
      Serial.println("Subscribed to: " + String(MQTT_TOPIC));
      is_update_qr_user = false;
    } else {
      Serial.printf("Failed, rc=%d. Trying again in 5 seconds.\n", client.state());
    }
  }
}

void send_message_mqtt(String msg) {
  Serial.println("Send message: " + msg);
  client.publish(MQTT_TOPIC, msg.c_str());
}

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

//----------------------------------
void save_id_door(String macAddress, String idDoor) {
  if (my_mac_address != macAddress)
    return;
  preferences.putString(PREF_ID_DOOR, idDoor);
}

//---------- LVGL Functions ----------
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

void log_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void show_register_card_text(bool is_register) {
  const char *text = is_register
                       ? "Register RFID: Waiting for card ..."
                       : "Remove RFID: Waiting for card ...";

  lv_label_set_text(registerCardTxt, text);
  lv_obj_clear_flag(registerCardTxt, LV_OBJ_FLAG_HIDDEN);
}

void hide_register_card_text() {
  lv_obj_add_flag(registerCardTxt, LV_OBJ_FLAG_HIDDEN);
}

void lv_create_tab_gui(void) {
  lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);

  lv_obj_t *tabUser = lv_tabview_add_tab(tabview, "User");
  lv_obj_t *tabManager = lv_tabview_add_tab(tabview, "Manager");

  lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
  lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

  qrUser = create_qrcode(tabUser, fg_color, bg_color);
  qrManager = create_qrcode(tabManager, fg_color, bg_color);

  registerCardTxt = lv_label_create(lv_screen_active());
  lv_obj_set_style_bg_color(registerCardTxt, bg_color, 0);
  lv_obj_set_size(registerCardTxt, 300, 80);
  lv_obj_set_style_text_align(registerCardTxt, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(registerCardTxt, LV_ALIGN_TOP_MID, 0, 0);
  hide_register_card_text();
}

lv_obj_t *create_qrcode(lv_obj_t *parent, lv_color_t fg_color, lv_color_t bg_color) {
  lv_obj_t *qrcode = lv_qrcode_create(parent);
  lv_qrcode_set_size(qrcode, 200);
  lv_qrcode_set_dark_color(qrcode, fg_color);
  lv_qrcode_set_light_color(qrcode, bg_color);
  lv_obj_center(qrcode);

  lv_obj_set_style_border_color(qrcode, bg_color, 0);
  lv_obj_set_style_border_width(qrcode, 5, 0);

  return qrcode;
}

void update_qr_user(const String &macAddress, const String &idDoor, const String &token) {
  if (my_mac_address != macAddress || preferences.getString(PREF_ID_DOOR, "") != idDoor)
    return;

  my_token = idDoor + "::" + token;
  lv_qrcode_update(qrUser, my_token.c_str(), my_token.length());
  is_update_qr_user = true;
}

void update_qr_manager() {
  String current_mac = WiFi.macAddress();
  if (my_mac_address != current_mac) {
    my_mac_address = current_mac;
    lv_qrcode_update(qrManager, current_mac.c_str(), current_mac.length());
    is_update_mac = false;
  }

  if (is_update_mac || !client.connected())
    return;

  String door_id = preferences.getString(PREF_ID_DOOR, "");
  if (door_id.isEmpty())
    return;

  String sendMsg = String(FROM_ESP) + "::" + String(UPDATE_MAC) + "::" + my_mac_address + "::" + door_id;
  send_message_mqtt(sendMsg);
  is_update_mac = true;
}

//---------- Another Functions ----------

void access_door(String macAddress, String idDoor) {
  if (my_mac_address != macAddress)
    return;
  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor != idDoor)
    return;
  open_door();
}

void open_door() {
  is_accept = true;
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(RELAY_PIN, LOW);
}

void check_token() {
  if (!client.connected() || is_update_qr_user)
    return;

  String myIdDoor = preferences.getString(PREF_ID_DOOR, "");
  if (myIdDoor.isEmpty()) {
    send_message_mqtt(String(FROM_ESP) + "::" + String(REQUEST_ID_DOOR) + "::" + my_mac_address);
    delay(2000 / portTICK_PERIOD_MS);
    return;
  }

  send_message_mqtt(String(FROM_ESP) + "::" + String(CHECK_TOKEN) + "::" + my_mac_address + "::" + myIdDoor);
  delay(2000 / portTICK_PERIOD_MS);
}

void get_response_check_token(String macAddress, String idDoor, String response) {
  if (my_mac_address != macAddress)
    return;
  if (preferences.getString(PREF_ID_DOOR, "") != idDoor)
    return;

  if (response == "TokenInvalid") {
    is_update_qr_user = false;
  } else {
    update_qr_user(macAddress, idDoor, response);
  }
}

//----------------------------------

//---------- Task ----------

void mqtt_task(void *pvParameters) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    if (!client.connected()) {
      reconnect_mqtt();
    }
    client.loop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void whistle_task(void *pvParameters) {
  while (true) {
    if (is_accept) {
      digitalWrite(WHISTLE_PIN, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(WHISTLE_PIN, LOW);
      vTaskDelay(150 / portTICK_PERIOD_MS);

      digitalWrite(WHISTLE_PIN, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(WHISTLE_PIN, LOW);

      is_accept = false;
      is_reading_card = false;
    } else if (is_deny) {
      digitalWrite(WHISTLE_PIN, HIGH);
      vTaskDelay(800 / portTICK_PERIOD_MS);
      digitalWrite(WHISTLE_PIN, LOW);

      is_deny = false;
      is_reading_card = false;
    } else {
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}

//----------------------------------

//---------- Setup ----------
void setup_rfid() {
  rdm6300.begin(RDM6300_RX_PIN);

  SPI.begin();
  rfid.PCD_Init();
}

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

void setup_mqtt() {
  String mqtt_server = preferences.getString(PREF_IP_MQTT, "");
  if (mqtt_server.isEmpty()) {
    client.setServer(MQTT_SERVER, MQTT_PORT);
  } else if (ip.fromString(mqtt_server)) {
    client.setServer(ip, MQTT_PORT);
  }

  client.setCallback(callback);
}

void setup_lvgl() {
  lv_init();
  lv_log_register_print_cb(log_print);

  // init SPI for lvgl
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

void setup() {
  Serial.begin(115200);

  preferences.begin("my-esp", false);
  SerialBT.begin("ESP32_LVGL");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(WHISTLE_PIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);

  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_BLUE_PIN, HIGH);

  setup_rfid();
  setup_lvgl();
  setup_wifi();
  setup_mqtt();

  xTaskCreate(mqtt_task, "MQTT Task", 2048, NULL, 1, NULL);
  xTaskCreate(whistle_task, "WHISTLE Task", 1024, NULL, 1, NULL);
}

//----------------------------------
void main_loop() {
  scan_rfid();

  if (SerialBT.isReady()) {
    receive_bluetooth_data();
  }

  if (WiFi.status() != WL_CONNECTED) {
    handle_wifi_reconnect();
  }

  update_qr_manager();
  check_token();
}

void loop() {
  lv_task_handler();
  lv_tick_inc(5);
  delay(5);
  main_loop();
}
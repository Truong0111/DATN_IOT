#include <MFRC522.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define RST_PIN 4  // Chân RST
#define SS_PIN 5

#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

#define RFID_MOSI 23
#define RFID_MISO 19
#define RFID_SCK 18
#define RFID_SDA 5

MFRC522 rfid(SS_PIN, RST_PIN);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23, SS_PIN);
  rfid.PCD_Init();
  ts.begin();
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.print("Touch at X: ");
    Serial.print(p.x);
    Serial.print(" Y: ");
    Serial.println(p.y);
  }

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("MFRC522 UID: ");

  char uid_str[3 * rfid.uid.size + 1];
  for (size_t i = 0; i < rfid.uid.size; i++) {
    snprintf(uid_str + i * 2, 3, "%02X", rfid.uid.uidByte[i]);
  }

  Serial.println(uid_str);

  rfid.PICC_HaltA();
}

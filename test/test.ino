#include <WiFi.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_BT_Config"); // Tên Bluetooth
  Serial.println("Bluetooth Started! Ready to pair.");
}

void loop() {
  if (SerialBT.available()) {
    char received[100]; // Dữ liệu nhận (giới hạn 100 ký tự)
    int len = SerialBT.readBytesUntil('\n', received, sizeof(received) - 1);
    received[len] = '\0'; // Kết thúc chuỗi

    Serial.println("Received: ");
    Serial.println(received);

    // Tách SSID và password từ chuỗi
    char* separator = strchr(received, '|');
    if (separator != nullptr) {
      *separator = '\0'; // Thay '|' bằng NULL để chia chuỗi
      char* ssid = received;
      char* password = separator + 1;

      connectToWiFi(ssid, password);
    }
  }
}

void connectToWiFi(const char* ssid, const char* password) {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    SerialBT.println("WiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed.");
    SerialBT.println("WiFi connection failed.");
  }
}

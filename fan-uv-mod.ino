/*
 * ESP32-C3 Super Mini - Fan Control with UV LED
 * - Nút quạt: Click để tăng cấp độ (OFF -> 35% -> 50% -> 75% -> 100% -> OFF...)
 * - Nút UV: Click để bật/tắt đèn UV
 * - Nút UV: Giữ 5 giây để bật/tắt WiFi AP mode cho upload code OTA
 * - Quạt và đèn UV không chạy cùng lúc
 * - Logic UV_PIN: HIGH = BẬT, LOW = TẮT
 * - Logic FAN_POWER_PIN: HIGH = BẬT, LOW = TẮT
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

// Định nghĩa chân (5 chân: 2 nút input, 2 bật/tắt output, 1 PWM output)
const int UV_BUTTON_PIN = 5;   // Nút điều khiển UV (INPUT)
const int UV_PIN = 2;          // Chân bật/tắt đèn UV (OUTPUT)
const int FAN_BUTTON_PIN = 6;  // Nút điều khiển quạt (INPUT)
const int FAN_POWER_PIN = 10;  // Chân bật/tắt nguồn quạt (OUTPUT)
const int FAN_PWM_PIN = 3;     // Chân PWM điều khiển tốc độ quạt (OUTPUT PWM)

// Cấu hình PWM
const int PWM_FREQ = 25000;  // Tần số PWM 25kHz
const int PWM_CHANNEL = 0;   // Kênh PWM
const int PWM_RESOLUTION = 8; // Độ phân giải 8-bit (0-255)

// Biến trạng thái
int fanLevel = 0;            // Mức độ quạt hiện tại (0=OFF, 1=35%, 2=50%, 3=75%, 4=100%)
bool fanOn = false;          // Trạng thái quạt (bật/tắt)
bool uvOn = false;           // Trạng thái đèn UV (bật/tắt)

// Biến cho nút quạt
bool lastFanButtonState = HIGH;
bool fanButtonState = HIGH;
unsigned long lastFanDebounceTime = 0;
unsigned long lastFanClickTime = 0;
bool waitingForFanDoubleClick = false;

// Biến cho nút UV
bool lastUvButtonState = HIGH;
bool uvButtonState = HIGH;
unsigned long lastUvDebounceTime = 0;
unsigned long uvButtonPressTime = 0;
bool uvButtonHeld = false;

// WiFi AP mode
bool wifiAPMode = false;

const unsigned long debounceDelay = 50;      // Thời gian chống dội nút bấm (ms)
const unsigned long doubleClickTime = 300;   // Thời gian tối đa giữa 2 lần nhấn (ms)
const unsigned long longPressTime = 5000;    // Thời gian giữ nút để bật/tắt WiFi AP (5 giây)

// Forward declarations
void IRAM_ATTR tachInterrupt();

// Mức PWM tương ứng với từng cấp độ (1=35%, 2=50%, 3=75%, 4=100%, 0=OFF)
const int PWM_LEVELS[] = {
  0,      // 0: OFF
  90,     // 1: 35% (255 * ~0.35)
  128,    // 2: 50% (255 * 0.5)
  191,    // 3: 75% (255 * 0.75)
  255     // 4: 100% (255 * 1.0)
};

void setup() {
  // Khởi tạo Serial để debug
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32-C3 Fan Control with UV LED ===");
  
  // Cấu hình chân nút bấm
  pinMode(FAN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(UV_BUTTON_PIN, INPUT_PULLUP);
  
  // Cấu hình chân điều khiển UV
  pinMode(UV_PIN, OUTPUT);
  digitalWrite(UV_PIN, LOW); // Tắt UV ban đầu (LOW = OFF)
  
  // Cấu hình chân điều khiển nguồn quạt
  pinMode(FAN_POWER_PIN, OUTPUT);
  digitalWrite(FAN_POWER_PIN, LOW); // Tắt nguồn quạt ban đầu (LOW = OFF)
  
  // Cấu hình PWM (sử dụng API mới cho ESP32 Arduino Core 3.x)
  ledcAttach(FAN_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  
  // Tắt quạt ban đầu (0%)
  setFanSpeed(0);
  Serial.println("Fan: OFF");
  Serial.println("UV LED: OFF");
  Serial.println("\nControls:");
  Serial.println("- Fan button: Single click = cycle speed (OFF->35%->50%->75%->100%), Double click = OFF");
  Serial.println("- UV button: Click to toggle UV LED");
  Serial.println("- UV button: Hold 5 seconds to enable/disable WiFi AP mode for OTA upload");
  Serial.println("- Fan and UV cannot run simultaneously");
  Serial.println("- UV_PIN: HIGH=ON, LOW=OFF");
  Serial.println("- FAN_POWER_PIN: HIGH=ON, LOW=OFF");
}

void loop() {
  // Xử lý WiFi AP mode nếu đã bật
  if (wifiAPMode) {
    ArduinoOTA.handle();
    // Vẫn kiểm tra nút UV để có thể thoát AP mode
    handleUvButton();
    return;
  }
  
  // Xử lý nút điều khiển quạt
  handleFanButton();
  
  // Xử lý nút điều khiển UV
  handleUvButton();
  
  // Kiểm tra timeout cho single click của nút quạt
  if (waitingForFanDoubleClick && (millis() - lastFanClickTime > doubleClickTime)) {
    waitingForFanDoubleClick = false;
    cycleFanSpeed();
  }
}

// Xử lý nút quạt
void handleFanButton() {
  int reading = digitalRead(FAN_BUTTON_PIN);
  
  if (reading != lastFanButtonState) {
    lastFanDebounceTime = millis();
  }
  
  if ((millis() - lastFanDebounceTime) > debounceDelay) {
    if (reading != fanButtonState) {
      fanButtonState = reading;
      
      if (fanButtonState == LOW) {
        unsigned long currentTime = millis();
        
        if (waitingForFanDoubleClick && (currentTime - lastFanClickTime < doubleClickTime)) {
          // Double click detected - Tắt quạt ngay
          waitingForFanDoubleClick = false;
          turnOffFan();
        } else {
          // Potential single click - wait to see if there's another click
          waitingForFanDoubleClick = true;
          lastFanClickTime = currentTime;
        }
      }
    }
  }
  
  lastFanButtonState = reading;
}

// Xử lý nút UV
void handleUvButton() {
  int reading = digitalRead(UV_BUTTON_PIN);
  
  // Phát hiện khi nút được nhấn xuống
  if (reading == LOW && lastUvButtonState == HIGH) {
    uvButtonPressTime = millis();
    uvButtonHeld = false;
  }
  
  // Kiểm tra nếu nút đang được giữ
  if (reading == LOW && !uvButtonHeld) {
    if (millis() - uvButtonPressTime >= longPressTime) {
      // Nút được giữ đủ 5 giây - Toggle WiFi AP
      uvButtonHeld = true;
      if (wifiAPMode) {
        disableWiFiAP();
      } else {
        enableWiFiAP();
      }
    }
  }
  
  if (reading != lastUvButtonState) {
    lastUvDebounceTime = millis();
  }
  
  if ((millis() - lastUvDebounceTime) > debounceDelay) {
    if (reading != uvButtonState) {
      uvButtonState = reading;
      
      // Chỉ toggle UV nếu là nhấn ngắn (thả nút trước 5 giây) và không ở chế độ AP
      if (uvButtonState == HIGH && !uvButtonHeld && !wifiAPMode) {
        if (millis() - uvButtonPressTime < longPressTime) {
          toggleUV();
        }
      }
    }
  }
  
  lastUvButtonState = reading;
}

// Xử lý click nút quạt - Tăng cấp độ tốc độ
void cycleFanSpeed() {
  Serial.println("\n*** Fan Button Pressed ***");
  
  // Nếu UV đang bật, tắt UV trước
  if (uvOn) {
    uvOn = false;
    digitalWrite(UV_PIN, LOW); // LOW = OFF
    Serial.println(">>> UV LED turned OFF (conflict with Fan)");
  }
  
  // Chu kỳ: 0 (OFF) -> 1 (35%) -> 2 (50%) -> 3 (75%) -> 4 (100%) -> 0 (OFF)
  fanLevel++;
  if (fanLevel > 4) {
    fanLevel = 0;
  }
  
  if (fanLevel == 0) {
    // Tắt quạt
    fanOn = false;
    setFanSpeed(0);
    digitalWrite(FAN_POWER_PIN, LOW); // LOW = OFF
    Serial.println(">>> Fan: OFF");
  } else {
    // Bật quạt với cấp độ tương ứng
    fanOn = true;
    digitalWrite(FAN_POWER_PIN, HIGH); // HIGH = ON
    setFanSpeed(fanLevel);
    Serial.print(">>> Fan: ON at ");
    Serial.print(fanLevel * 25);
    Serial.print("% | PWM: ");
    Serial.println(PWM_LEVELS[fanLevel]);
  }
}

// Hàm tắt quạt (dùng cho double click)
void turnOffFan() {
  Serial.println("\n*** Fan Double Click - Turn OFF ***");
  
  fanOn = false;
  fanLevel = 0;
  setFanSpeed(0);
  digitalWrite(FAN_POWER_PIN, LOW); // LOW = OFF
  Serial.println(">>> Fan: OFF");
}

// Xử lý nút UV - Bật/tắt đèn UV
void toggleUV() {
  Serial.println("\n*** UV Button Pressed ***");
  
  uvOn = !uvOn;
  
  if (uvOn) {
    // Nếu quạt đang bật, tắt quạt trước
    if (fanOn) {
      fanOn = false;
      fanLevel = 0;
      setFanSpeed(0);
      digitalWrite(FAN_POWER_PIN, LOW); // LOW = OFF
      Serial.println(">>> Fan turned OFF (conflict with UV)");
    }
    
    // Bật UV
    digitalWrite(UV_PIN, HIGH); // HIGH = ON
    Serial.println(">>> UV LED: ON");
  } else {
    // Tắt UV
    digitalWrite(UV_PIN, LOW); // LOW = OFF
    Serial.println(">>> UV LED: OFF");
  }
}

// Hàm thiết lập tốc độ quạt
void setFanSpeed(int level) {
  if (level < 0 || level > 4) {
    level = 0; // Giới hạn trong phạm vi hợp lệ
  }
  
  ledcWrite(FAN_PWM_PIN, PWM_LEVELS[level]);
}

// Hàm bật WiFi AP mode với OTA
void enableWiFiAP() {
  Serial.println("\n*** ENABLING WIFI AP MODE WITH OTA ***");
  
  // Tắt quạt và UV
  if (fanOn) {
    turnOffFan();
  }
  if (uvOn) {
    uvOn = false;
    digitalWrite(UV_PIN, LOW);
  }
  
  // Bật WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FAN-UV-MOD", "12345678");
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("\nAP IP address: ");
  Serial.println(IP);
  Serial.println("SSID: FAN-UV-MOD");
  Serial.println("Password: 12345678");
  
  // Cấu hình ArduinoOTA
  ArduinoOTA.setHostname("ESP32-Fan");
  ArduinoOTA.setPassword("12345678");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  
  Serial.println("\n=== OTA Ready ===");
  Serial.println("Arduino IDE: Tools -> Port -> ESP32-Fan");
  Serial.println("Hold UV button 5s again to exit AP mode");
  
  wifiAPMode = true;
  
  // Nhấp nháy LED UV để báo hiệu đã vào chế độ WiFi AP
  for (int i = 0; i < 6; i++) {
    digitalWrite(UV_PIN, HIGH);
    delay(200);
    digitalWrite(UV_PIN, LOW);
    delay(200);
  }
}

// Hàm tắt WiFi AP mode
void disableWiFiAP() {
  Serial.println("\n*** DISABLING WIFI AP MODE ***");
  
  // Tắt OTA và WiFi
  ArduinoOTA.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  
  wifiAPMode = false;
  
  Serial.println("WiFi AP disabled. Normal operation resumed.");
  
  // Nhấp nháy LED UV để báo hiệu đã thoát chế độ WiFi AP
  for (int i = 0; i < 3; i++) {
    digitalWrite(UV_PIN, HIGH);
    delay(100);
    digitalWrite(UV_PIN, LOW);
    delay(100);
  }
}

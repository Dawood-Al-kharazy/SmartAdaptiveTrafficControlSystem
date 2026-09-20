#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ====================================================================
// 1. إعدادات الشبكة و MQTT (Mosquitto Local Broker)
// ====================================================================
// اكتب هنا اسم شبكة الواي فاي (أو نقطة اتصال هاتفك) وكلمة المرور
#define WIFI_SSID "*****"
#define WIFI_PASSWORD "12121212"

// عنوان الـ IP لجهاز اللابتوب الذي يعمل عليه Mosquitto
// (يمكنك معرفته بكتابة ipconfig في Command Prompt)
const char* mqtt_server = "192.168.43.99"; 
const int   mqtt_port   = 1883;

// قنوات الـ MQTT (مطابقة تماماً للـ Node-RED)
const char* TOPIC_DATA    = "sanad/traffic/data";     // لنشر حالة الإشارات وعدد السيارات
const char* TOPIC_COMMAND = "sanad/traffic/command";  // للاشتراك واستقبال أوامر الطوارئ

WiFiClient espClient;
PubSubClient client(espClient);

// ====================================================================
// 2. تعريف منافذ الإشارات وحساسات السيارات
// ====================================================================
// مصفوفة الليدات [المسار][اللون: 0=أحمر, 1=أصفر, 2=أخضر]
const uint8_t L[4][3] = {
  {13, 12, 14}, // North (Red=13, Yellow=12, Green=14)
  {26, 25, 33}, // East  (Red=26, Yellow=25, Green=33)
  {15, 2,  4 }, // South (Red=15, Yellow=2,  Green=4 )
  {18, 19, 21}  // West  (Red=18, Yellow=19, Green=21)
};

// منافذ حساسات الـ IR (تعمل بالمقاطعة Interrupts)
// ملاحظة: إذا كان البورد يحتوي مسارين فقط، يتم توصيل أول مسارين فقط (North & East)
const uint8_t BTNS[4] = {27, 32, 5, 22}; 

// ====================================================================
// 3. متغيرات النظام والتحكم
// ====================================================================
volatile int cars[4] = {0, 0, 0, 0}; 
volatile unsigned long lastDebounce[4] = {0, 0, 0, 0};

int currentState = 0;          // الحالة الحالية (0 إلى 7)
int interruptedState = 0;      // الحالة المحفوظة عند قطع المسار لحالة طوارئ
unsigned long prevMillis = 0; 
long greenTime = 5000;         // وقت الأخضر الافتراضي
const long Y_TIME  = 2000;     // وقت الإشارة الصفراء (ثانيتان)
const long EM_TIME = 10000;    // وقت إشارة الطوارئ للإسعاف (10 ثوانٍ)

int emLane = 0;                // رقم مسار الطوارئ (1=North, 2=East, 3=South, 4=West)
bool isEmMode = false;         // راية تدل على تفعيل وضع الطوارئ

// ====================================================================
// 4. دوال المقاطعة الحساسة (Hardware Interrupts) لحساب السيارات
// ====================================================================
// الحساس يحسب السيارة فقط إذا لم تكن إشارته خضراء حالياً
void IRAM_ATTR nISR() { 
  if (millis() - lastDebounce[0] > 250 && currentState >= 2) { 
    cars[0]++; 
    lastDebounce[0] = millis(); 
  }
}

void IRAM_ATTR eISR() { 
  if (millis() - lastDebounce[1] > 250 && (currentState <= 1 || currentState >= 4)) { 
    cars[1]++; 
    lastDebounce[1] = millis(); 
  }
}

void IRAM_ATTR sISR() { 
  if (millis() - lastDebounce[2] > 250 && (currentState <= 3 || currentState >= 6)) { 
    cars[2]++; 
    lastDebounce[2] = millis(); 
  }
}

void IRAM_ATTR wISR() { 
  if (millis() - lastDebounce[3] > 250 && currentState <= 5) { 
    cars[3]++; 
    lastDebounce[3] = millis(); 
  }
}

// تعريفات مسبقة للدوال
void triggerEmergency(int lane);
void startEmGreen();
void transitionTo(int next);
void applyLights(int state);
long getWaitTime(int c);

// ====================================================================
// 5. استقبال أوامر الـ MQTT (Callback)
// ====================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  Serial.print("[MQTT Recv] Topic: ");
  Serial.print(topic);
  Serial.print(" | Message: ");
  Serial.println(msg);

  // استخراج رقم المسار سواء وصل رقماً عادياً 1 أو كائن JSON مثل {"lane": 1}
  int cmd = 0;
  if (msg.startsWith("{")) {
    JsonDocument cmdDoc;
    DeserializationError err = deserializeJson(cmdDoc, msg);
    if (!err) {
      cmd = cmdDoc["lane"] | cmdDoc["emergencyLane"] | 0;
    }
  } else {
    cmd = msg.toInt();
  }

  if (cmd >= 1 && cmd <= 4 && !isEmMode) {
    Serial.printf(">>> Activating Emergency for Lane %d <<<\n", cmd);
    triggerEmergency(cmd);
  }
}

// إعادة الاتصال بالـ MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to Mosquitto MQTT...");
    String clientId = "ESP32_Traffic_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" CONNECTED!");
      client.subscribe(TOPIC_COMMAND);
      Serial.printf("Subscribed to: %s\n", TOPIC_COMMAND);
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 1 second...");
      delay(1000);
    }
  }
}

// ====================================================================
// 6. الإعداد الأولي (Setup)
// ====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Smart Adaptive Traffic Control System (IoT) ===");

  // تهيئة المنافذ
  for (int i = 0; i < 4; i++) {
    pinMode(BTNS[i], INPUT_PULLUP);
    for (int c = 0; c < 3; c++) {
      pinMode(L[i][c], OUTPUT);
    }
  }

  // تفعيل المقاطعات للحساسات الأربعة
  attachInterrupt(digitalPinToInterrupt(BTNS[0]), nISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTNS[1]), eISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTNS[2]), sISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTNS[3]), wISR, FALLING);

  // الاتصال بالواي فاي
  Serial.printf("Connecting to Wi-Fi: %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // تهيئة خادم MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // بدء الإشارة بالحالة 0 (أخضر شمال)
  applyLights(0);
}

// ====================================================================
// 7. الحلقة الرئيسية (Loop)
// ====================================================================
void loop() {
  // الحفاظ على استقرار الواي فاي
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost! Reconnecting...");
    WiFi.reconnect();
    delay(500);
  }

  // الحفاظ على استقرار MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  unsigned long now = millis();

  // -------------------------------------------------------------
  // أ) معالجة وضع الطوارئ
  // -------------------------------------------------------------
  if (isEmMode) {
    int targetGreen = (emLane - 1) * 2;
    int targetYellow = targetGreen + 1;

    if (currentState == targetGreen) {
      if (now - prevMillis >= EM_TIME) {
        Serial.println("Emergency Green Finished -> Yellow Warning");
        transitionTo(targetYellow);
      }
    } 
    else if (currentState == targetYellow) {
      if (now - prevMillis >= Y_TIME) {
        Serial.println("Emergency Finished -> Restoring Normal Cycle");
        isEmMode = false;
        if (interruptedState % 2 == 0) {
          greenTime = getWaitTime(cars[interruptedState / 2]);
        }
        transitionTo(interruptedState);
      }
    } 
    else {
      if (now - prevMillis >= Y_TIME) {
        startEmGreen();
      }
    }
  }

  // -------------------------------------------------------------
  // ب) إرسال البيانات فورياً عند تغير الإشارة أو دورياً كل ثانية
  // -------------------------------------------------------------
  static unsigned long lastSync = 0;
  static int lastSyncedState = -1;

  if (currentState != lastSyncedState || now - lastSync >= 1000) {
    // متوافق مع ArduinoJson v6 و v7
    JsonDocument doc;
    doc["nCars"] = cars[0];
    doc["eCars"] = cars[1];
    doc["sCars"] = cars[2];
    doc["wCars"] = cars[3];
    doc["state"] = currentState;
    doc["emergencyLane"] = isEmMode ? emLane : 0;

    char jsonString[200];
    serializeJson(doc, jsonString);
    client.publish(TOPIC_DATA, jsonString);
    Serial.printf("[MQTT Publish] %s\n", jsonString);

    lastSyncedState = currentState;
    lastSync = now;
  }

  // -------------------------------------------------------------
  // ج) الدورة الطبيعية للإشارات وفق الخوارزمية التكيفية
  // -------------------------------------------------------------
  if (!isEmMode || (isEmMode && currentState % 2 != 0)) {
    long waitTime = (currentState % 2 != 0) ? Y_TIME : greenTime;

    if (now - prevMillis >= waitTime) {
      int next = (currentState + 1) % 8;

      if (currentState % 2 != 0) { // كنا في الأصفر وننتقل للأخضر التالي
        if (isEmMode) {
          startEmGreen();
          return;
        } else {
          int nextLaneIndex = next / 2;
          // احتساب وقت الأخضر بناءً على سيارات المسار القادم
          greenTime = getWaitTime(cars[nextLaneIndex]);
          // تصفير عداد سيارات المسار الذي سيفتح أخضر
          cars[nextLaneIndex] = 0;
        }
      }
      transitionTo(next);
    }
  }
}

// ====================================================================
// 8. الدوال المساعدة (Helper Functions)
// ====================================================================

// تفعيل الطوارئ
void triggerEmergency(int lane) {
  emLane = lane;
  isEmMode = true;
  interruptedState = currentState;

  int targetState = (lane - 1) * 2;
  if (currentState == targetState) {
    prevMillis = millis();
    greenTime = EM_TIME; // تمديد الوقت
  } else {
    // الانتقال عبر الأصفر أولاً لتفريغ التقاطع بأمان
    if (currentState % 2 == 0) {
      transitionTo(currentState + 1);
    } else {
      startEmGreen();
    }
  }
}

// بدء الإشارة الخضراء للطوارئ
void startEmGreen() {
  int target = (emLane - 1) * 2;
  currentState = target;
  greenTime = EM_TIME;
  prevMillis = millis();
  applyLights(currentState);
  Serial.printf(">>> LANE %d GREEN OPEN FOR EMERGENCY <<<\n", emLane);
}

// التبديل إلى حالة جديدة
void transitionTo(int next) {
  currentState = next;
  prevMillis = millis();
  applyLights(currentState);
}

// تشغيل الليدات وفق الحالة
void applyLights(int state) {
  int activeLane = state / 2;     // 0=N, 1=E, 2=S, 3=W
  bool isYellow = state % 2 != 0; 

  for (int i = 0; i < 4; i++) {
    if (i == activeLane) {
      digitalWrite(L[i][0], LOW);          // إطفاء الأحمر
      digitalWrite(L[i][1], isYellow);     // تشغيل الأصفر إذا كانت الحالة صفراء
      digitalWrite(L[i][2], !isYellow);    // تشغيل الأخضر إذا لم تكن صفراء
    } else {
      digitalWrite(L[i][0], HIGH);         // تشغيل الأحمر لباقي المسارات
      digitalWrite(L[i][1], LOW);
      digitalWrite(L[i][2], LOW);
    }
  }
}

// الخوارزمية التكيفية لتحديد مدة الإشارة الخضراء
long getWaitTime(int c) {
  if (c == 0) return 5000;   // 5 ثوانٍ إذا لا توجد سيارات
  if (c <= 3) return 15000;  // 15 ثانية من 1 إلى 3 سيارات
  return 25000;              // 25 ثانية إذا أكثر من 3 سيارات
}

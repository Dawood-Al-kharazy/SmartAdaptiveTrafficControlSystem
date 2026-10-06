#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ====================================================================
// 1. إعدادات الشبكة و MQTT (Mosquitto Local Broker)
// ====================================================================
#define WIFI_SSID "majed"
#define WIFI_PASSWORD "12345678"

const char* mqtt_server = "192.168.43.100"; 
const int   mqtt_port   = 1883;

const char* TOPIC_DATA    = "sanad/traffic/data";     // لنشر حالة الكثافة والإشارات
const char* TOPIC_COMMAND = "sanad/traffic/command";  // لاستقبال أوامر الطوارئ

WiFiClient espClient;
PubSubClient client(espClient);

// ====================================================================
// 2. تعريف منافذ إشارات المرور (12 دايود ضوئي)
// ====================================================================
// [المسار: 0=شمال, 1=شرق, 2=جنوب, 3=غرب][اللون: 0=أحمر, 1=أصفر, 2=أخضر]
const uint8_t L[4][3] = {
  {13, 12, 14}, // North (Red=13, Yellow=12, Green=14)
  {26, 25, 33}, // East  (Red=26, Yellow=25, Green=33)
  {15, 2,  4 }, // South (Red=15, Yellow=2,  Green=4 )
  {18, 19, 21}  // West  (Red=18, Yellow=19, Green=21)
};

// ====================================================================
// 3. تعريف منافذ حساسات الكثافة (حساسين لكل مسار = 8 حساسات)
// ====================================================================
// [المسار][0=حساس خط التوقف Stop-Line, 1=حساس نهاية المسار Queue-Tail]
const uint8_t DENSITY_PINS[4][2] = {
  {27, 32}, // North: Stop=27, Tail=32
  {34, 35}, // East:  Stop=34, Tail=35 (Input Only)
  {36, 39}, // South: Stop=36, Tail=39 (Input Only)
  {5,  22}  // West:  Stop=5,  Tail=22
};

// ====================================================================
// 4. منفذ حساس استشعار الإسعاف (حساس FR/IR واحد في المسار المخصص)
// ====================================================================
const uint8_t EM_SENSOR_PIN = 23; // منفذ حساس الإسعاف (IR/Flame FR Module)
const int     EM_LANE_NUM   = 1;  // المسار المخصص للإسعاف في المجسم (1 = الشمال North)

// ====================================================================
// 5. متغيرات النظام وحساب الكثافة
// ====================================================================
enum DensityLevel { EMPTY = 0, LOW_DENSITY = 1, MEDIUM_DENSITY = 2, HIGH_DENSITY = 3 };

DensityLevel laneDensity[4] = {EMPTY, EMPTY, EMPTY, EMPTY};
long allocatedGreenTime[4]  = {3000, 3000, 3000, 3000};
unsigned long stopDetectTime[4] = {0, 0, 0, 0};

int currentState = 0;          // 0 إلى 7 (حالات الإشارات)
int interruptedState = 0;      // لحفظ الدورة عند حدوث طوارئ
unsigned long prevMillis = 0; 
long currentGreenDuration = 5000;
const long Y_TIME  = 2000;     // زمن الإشارة الصفراء (ثانيتان)
const long EM_TIME = 10000;    // زمن إشارة الإسعاف (10 ثوانٍ)

int emLane = 0;                // رقم مسار الطوارئ (1=North, 2=East, 3=South, 4=West)
bool isEmMode = false;

// تعريفات مسبقة للدوال
void updateDensities();
void triggerEmergency(int lane);
void startEmGreen();
void transitionTo(int next);
void applyLights(int state);

// ====================================================================
// 6. خوارزمية قياس الكثافة بنظام زمن الإشغال (Occupancy Evaluation)
// ====================================================================
void updateDensities() {
  unsigned long now = millis();

  for (int i = 0; i < 4; i++) {
    bool stopActive = (digitalRead(DENSITY_PINS[i][0]) == LOW); // حساس التوقف كشف سيارة
    bool tailActive = (digitalRead(DENSITY_PINS[i][1]) == LOW); // حساس نهاية المسار كشف سيارة

    // قياس زمن توقف السيارة عند الخط
    if (stopActive) {
      if (stopDetectTime[i] == 0) stopDetectTime[i] = now;
    } else {
      stopDetectTime[i] = 0;
    }

    // تصنيف مستوى الكثافة:
    if (tailActive) {
      // الطابور وصل لآخر الشارع -> اختناق مروري
      laneDensity[i] = HIGH_DENSITY;
      allocatedGreenTime[i] = 25000; // 25 ثانية
    } 
    else if (stopActive && (now - stopDetectTime[i] >= 2000)) {
      // السيارة متوقفة لأكثر من ثانيتين عند الخط -> كثافة متوسطة
      laneDensity[i] = MEDIUM_DENSITY;
      allocatedGreenTime[i] = 15000; // 15 ثانية
    } 
    else if (stopActive) {
      // وجود سيارة واحدة حديثة الوصول -> كثافة منخفضة
      laneDensity[i] = LOW_DENSITY;
      allocatedGreenTime[i] = 8000;  // 8 ثوانٍ
    } 
    else {
      // المسار فارغ تماماً -> تخطي أو حد أدنى
      laneDensity[i] = EMPTY;
      allocatedGreenTime[i] = 3000;  // 3 ثوانٍ فقط (توفير الوقت)
    }
  }
}

// فحص حساس الإسعاف المادي (IR / Flame FR Sensor)
void checkEmergencySensors() {
  if (isEmMode) return;

  if (digitalRead(EM_SENSOR_PIN) == LOW) {
    Serial.printf(">>> [Hardware FR Sensor] Emergency Detected on Lane %d! <<<\n", EM_LANE_NUM);
    triggerEmergency(EM_LANE_NUM);
  }
}


// ====================================================================
// 7. استقبال أوامر الـ MQTT
// ====================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.printf("[MQTT Recv] Topic: %s | Message: %s\n", topic, msg.c_str());

  int cmd = 0;
  if (msg.startsWith("{")) {
    JsonDocument cmdDoc;
    DeserializationError err = deserializeJson(cmdDoc, msg);
    if (!err) cmd = cmdDoc["lane"] | cmdDoc["emergencyLane"] | 0;
  } else {
    cmd = msg.toInt();
  }

  if (cmd >= 1 && cmd <= 4 && !isEmMode) {
    Serial.printf(">>> [MQTT Command] Activating Emergency for Lane %d <<<\n", cmd);
    triggerEmergency(cmd);
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to Mosquitto MQTT...");
    String clientId = "ESP32_Traffic_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" CONNECTED!");
      client.subscribe(TOPIC_COMMAND);
    } else {
      delay(1000);
    }
  }
}

// ====================================================================
// 8. الإعداد الأولي (Setup)
// ====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Smart Adaptive Traffic Control System (Diorama Edition) ===");

  // تهيئة الليدات
  for (int i = 0; i < 4; i++) {
    for (int c = 0; c < 3; c++) {
      pinMode(L[i][c], OUTPUT);
    }
  }

  // تهيئة حساسات الكثافة (8 حساسات)
  for (int i = 0; i < 4; i++) {
    pinMode(DENSITY_PINS[i][0], INPUT_PULLUP);
    pinMode(DENSITY_PINS[i][1], INPUT_PULLUP);
  }

  // تهيئة حساس الإسعاف المادي (حساس واحد)
  pinMode(EM_SENSOR_PIN, INPUT_PULLUP);

  // الاتصال بالواي فاي
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  applyLights(0);
}

// ====================================================================
// 9. الحلقة الرئيسية (Loop)
// ====================================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(500);
  }

  if (!client.connected()) reconnectMQTT();
  client.loop();

  // فحص الكثافة وحساسات الإسعاف باستمرار
  updateDensities();
  checkEmergencySensors();

  unsigned long now = millis();

  // -------------------------------------------------------------
  // أ) معالجة وضع الطوارئ
  // -------------------------------------------------------------
  if (isEmMode) {
    int targetGreen = (emLane - 1) * 2;
    int targetYellow = targetGreen + 1;

    if (currentState == targetGreen) {
      if (now - prevMillis >= EM_TIME) {
        Serial.println("Emergency Green Done -> Transitioning to Yellow");
        transitionTo(targetYellow);
      }
    } 
    else if (currentState == targetYellow) {
      if (now - prevMillis >= Y_TIME) {
        Serial.println("Emergency Clearance Done -> Restoring Normal Cycle");
        isEmMode = false;
        transitionTo(interruptedState);
      }
    } 
    else {
      if (now - prevMillis >= Y_TIME) startEmGreen();
    }
  }

  // -------------------------------------------------------------
  // ب) النشر الدوري لحالة الكثافة والإشارات عبر MQTT (كل ثانية)
  // -------------------------------------------------------------
  static unsigned long lastSync = 0;
  static int lastSyncedState = -1;

  if (currentState != lastSyncedState || now - lastSync >= 1000) {
    JsonDocument doc;
    const char* densityNames[] = {"EMPTY", "LOW", "MED", "HIGH"};
    
    doc["nDensity"] = densityNames[laneDensity[0]]; doc["nTime"] = allocatedGreenTime[0] / 1000;
    doc["eDensity"] = densityNames[laneDensity[1]]; doc["eTime"] = allocatedGreenTime[1] / 1000;
    doc["sDensity"] = densityNames[laneDensity[2]]; doc["sTime"] = allocatedGreenTime[2] / 1000;
    doc["wDensity"] = densityNames[laneDensity[3]]; doc["wTime"] = allocatedGreenTime[3] / 1000;
    
    // التوافق العكسي مع الداشبورد القديمة
    doc["nCars"] = (int)laneDensity[0];
    doc["eCars"] = (int)laneDensity[1];
    doc["sCars"] = (int)laneDensity[2];
    doc["wCars"] = (int)laneDensity[3];

    doc["state"] = currentState;
    doc["activeLane"] = currentState / 2;
    doc["emergencyLane"] = isEmMode ? emLane : 0;

    char jsonString[300];
    serializeJson(doc, jsonString);
    client.publish(TOPIC_DATA, jsonString);
    Serial.printf("[MQTT Publish] %s\n", jsonString);

    lastSyncedState = currentState;
    lastSync = now;
  }

  // -------------------------------------------------------------
  // ج) الدورة الطبيعية للإشارات مع ميزة Gap-Out والتوقيت التكيفي
  // -------------------------------------------------------------
  if (!isEmMode || (isEmMode && currentState % 2 != 0)) {
    long waitTime = (currentState % 2 != 0) ? Y_TIME : currentGreenDuration;

    // ميزة Gap-Out الذكية: إذا كان الشارع أخضر وخلا تماماً من السيارات، ننهي الأخضر مبكراً!
    int currentActiveLane = currentState / 2;
    if (currentState % 2 == 0 && now - prevMillis >= 4000) { // بعد مرور 4 ثوانٍ كحد أدنى
      if (laneDensity[currentActiveLane] == EMPTY) {
        Serial.printf(">>> Gap-Out Triggered: Lane %d is Empty! Ending Green early. <<<\n", currentActiveLane);
        waitTime = 0; // إنهاء فوري للأخضر
      }
    }

    if (now - prevMillis >= waitTime) {
      int next = (currentState + 1) % 8;

      if (currentState % 2 != 0) { // كنا في الأصفر وننتقل للأخضر التالي
        if (isEmMode) {
          startEmGreen();
          return;
        } else {
          int nextLaneIndex = next / 2;
          // احتساب زمن الأخضر للمسار الجديد ديناميكياً من مستوى كثافته
          currentGreenDuration = allocatedGreenTime[nextLaneIndex];
          Serial.printf("Switching to Lane %d (Density: %d) -> Green Duration: %ld ms\n", 
                        nextLaneIndex, (int)laneDensity[nextLaneIndex], currentGreenDuration);
        }
      }
      transitionTo(next);
    }
  }
}

// ====================================================================
// 10. الدوال المساعدة للطوارئ والتحكم
// ====================================================================
void triggerEmergency(int lane) {
  emLane = lane;
  isEmMode = true;
  interruptedState = currentState;

  int targetState = (lane - 1) * 2;
  if (currentState == targetState) {
    prevMillis = millis();
    currentGreenDuration = EM_TIME;
  } else {
    if (currentState % 2 == 0) transitionTo(currentState + 1);
    else startEmGreen();
  }
}

void startEmGreen() {
  int target = (emLane - 1) * 2;
  currentState = target;
  currentGreenDuration = EM_TIME;
  prevMillis = millis();
  applyLights(currentState);
  Serial.printf(">>> LANE %d GREEN OPEN FOR EMERGENCY <<<\n", emLane);
}

void transitionTo(int next) {
  currentState = next;
  prevMillis = millis();
  applyLights(currentState);
}

void applyLights(int state) {
  int activeLane = state / 2;
  bool isYellow = state % 2 != 0;

  for (int i = 0; i < 4; i++) {
    if (i == activeLane) {
      digitalWrite(L[i][0], LOW);
      digitalWrite(L[i][1], isYellow);
      digitalWrite(L[i][2], !isYellow);
    } else {
      digitalWrite(L[i][0], HIGH);
      digitalWrite(L[i][1], LOW);
      digitalWrite(L[i][2], LOW);
    }
  }
}

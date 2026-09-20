#include <WiFi.h>
#include <FirebaseESP32.h>

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define FIREBASE_HOST "adaptive-trafic-default-rtdb.europe-west1.firebasedatabase.app" 
#define FIREBASE_AUTH "34XdomffLowgob8aOK6QJeqQNLO4pavxhplOzJzo" 

FirebaseData fbData;
FirebaseConfig fbConfig;
FirebaseAuth fbAuth;

const uint8_t L[4][3] = {
  {13, 12, 14}, // North
  {26, 25, 33}, // East
  {15, 2, 4},   // South
  {18, 19, 21}  // West
};
const uint8_t BTNS[4] = {27, 32, 5, 22}; 

volatile int cars[4] = {0, 0, 0, 0}; 
volatile unsigned long lastDebounce[4] = {0, 0, 0, 0};

int currentState = 0; 
int interruptedState = 0;
unsigned long prevMillis = 0; 
long greenTime = 5000;  
const long Y_TIME = 2000; 
const long EM_TIME = 10000;

int emLane = 0; 
bool isEmMode = false;

void IRAM_ATTR nISR() { if(millis()-lastDebounce[0]>250 && currentState >= 2) { cars[0]++; lastDebounce[0]=millis(); }}
void IRAM_ATTR eISR() { if(millis()-lastDebounce[1]>250 && (currentState<=1 || currentState>=4)) { cars[1]++; lastDebounce[1]=millis(); }}
void IRAM_ATTR sISR() { if(millis()-lastDebounce[2]>250 && (currentState<=3 || currentState>=6)) { cars[2]++; lastDebounce[2]=millis(); }}
void IRAM_ATTR wISR() { if(millis()-lastDebounce[3]>250 && currentState <= 5) { cars[3]++; lastDebounce[3]=millis(); }}

void setup() {
  Serial.begin(115200);
  
  for(int i=0; i<4; i++){
    pinMode(BTNS[i], INPUT_PULLUP);
    for(int c=0; c<3; c++) pinMode(L[i][c], OUTPUT);
  }

  attachInterrupt(BTNS[0], nISR, FALLING); attachInterrupt(BTNS[1], eISR, FALLING);
  attachInterrupt(BTNS[2], sISR, FALLING); attachInterrupt(BTNS[3], wISR, FALLING);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  applyLights(0);
}

void loop() {
  unsigned long now = millis();

 if (isEmMode) {
     int targetGreen = (emLane - 1) * 2;
     int targetYellow = targetGreen + 1;

     //  نحن في الأخضر الخاص بالإسعاف
     if (currentState == targetGreen) {
        if (now - prevMillis >= EM_TIME) {
           Serial.println("EM Green Done -> Going to EM Yellow");
           transitionTo(targetYellow); // انتقل للأصفر الخاص بالإسعاف
        }
     }
     //  نحن في الأصفر الخاص بالإسعاف (مرحلة الخروج)
     else if (currentState == targetYellow) {
        if (now - prevMillis >= Y_TIME) {
           Serial.println("EM Yellow Done -> Restoring Normal Flow");
           isEmMode = false; // إنهاء الطوارئ
           
           // استعادة المسار القديم
           if (interruptedState % 2 == 0) {
              greenTime = getWaitTime(cars[interruptedState/2]);
           }
           transitionTo(interruptedState); 
        }
     }
     //  نحن في أصفر "ما قبل الإسعاف" (تنظيف التقاطع القديم)
     else {
        if (now - prevMillis >= Y_TIME) {
           startEmGreen(); // انتهى التنظيف، افتح أخضر الإسعاف
        }
     }
  }

// السحابة
  static unsigned long lastSync = 0;
  static int lastSyncedState = -1;
  
  // مزامنة فورية إذا تغيرت حالة الإشارة، أو مزامنة دورية كل ثانية لتحديث أعداد السيارات
  if (currentState != lastSyncedState || now - lastSync >= 1000) {
    FirebaseJson json;
    json.set("nCars", cars[0]); json.set("eCars", cars[1]);
    json.set("sCars", cars[2]); json.set("wCars", cars[3]);
    json.set("state", currentState);
    Firebase.updateNode(fbData, "/traffic", json); 
    
    lastSyncedState = currentState;
    lastSync = now;

    if (!isEmMode && Firebase.getInt(fbData, "/traffic/emergencyLane")) {
      int cmd = fbData.intData();
      if (cmd != 0) triggerEmergency(cmd);
    }
  }

  // لن يتم تنفيذ هذا الجزء إذا كنا في وضع الطوارئ لأننا نتحكم به في الأعلى
  // إلا إذا كنا ننتظر الانتقال من الأصفر للأخضر داخل الطوارئ
  if (!isEmMode || (isEmMode && currentState % 2 != 0)) {
      
      long waitTime = (currentState % 2 != 0) ? Y_TIME : greenTime;
      
      if (now - prevMillis >= waitTime) {
        int next = (currentState + 1) % 8;
        
        if (currentState % 2 != 0) { // كنا في الأصفر
           if(isEmMode) {
              startEmGreen(); // الانتقال من أصفر الطوارئ -> لأخضر الطوارئ
              return;
           } else {
              int nextLaneIndex = (next / 2); 
              greenTime = getWaitTime(cars[nextLaneIndex]); 
              cars[nextLaneIndex] = 0; 
           }
        }
        transitionTo(next);
      }
  }
}

// --- الدوال المساعدة ---

void triggerEmergency(int lane) {
  emLane = lane;
  isEmMode = true;
  interruptedState = currentState;
  Serial.printf("!!! Emergency: %d\n", lane);
  
  int targetState = (lane - 1) * 2;
  if (currentState == targetState) {
    prevMillis = millis(); greenTime = EM_TIME; // تمديد الوقت
  } else {
    // إذا لم يكن أصفر، انتقل للأصفر. إذا كان أصفر، نفذ فوراً
    if (currentState % 2 == 0) transitionTo(currentState + 1);
    else startEmGreen();
  }
}

void startEmGreen() {
  int target = (emLane - 1) * 2;
  currentState = target;
  greenTime = EM_TIME;
  prevMillis = millis();
  applyLights(currentState);
  Serial.println(">>> EM OPEN");
  Firebase.setInt(fbData, "/traffic/emergencyLane", 0);
}

void transitionTo(int next) {
  currentState = next;
  prevMillis = millis();
  applyLights(currentState);
}

// دالة الإضاءة "الذكية" - أسرع وأقصر
void applyLights(int state) {
  int activeLane = state / 2;     // 0, 1, 2, 3
  bool isYellow = state % 2 != 0; 

  for(int i=0; i<4; i++) {
    if(i == activeLane) {
      // المسار النشط: شغل الأخضر أو الأصفر، وأطفئ الأحمر
      digitalWrite(L[i][0], LOW);          // Red OFF
      digitalWrite(L[i][1], isYellow);     // Yellow ON if yellow state
      digitalWrite(L[i][2], !isYellow);    // Green ON if not yellow state
    } else {
      // باقي المسارات: أحمر فقط
      digitalWrite(L[i][0], HIGH); // Red ON
      digitalWrite(L[i][1], LOW);
      digitalWrite(L[i][2], LOW);
    }
  }
}

long getWaitTime(int c) {
  if (c == 0) return 5000;
  if (c <= 3) return 15000;
  return 25000;
}
# 🚦 IoT-Based Smart Adaptive Traffic Control System
**نظام التحكم المروري التكيفي الذكي القائم على إنترنت الأشياء (IoT)**

---

## 📌 نظرة عامة على المشروع (Project Overview)
مشروع إنترنت أشياء (IoT) متكامل لإدارة تقاطع مروري رباعي الاتجاهات بشكل ذكي وتكيفي:
1. **العتاد الميداني (ESP32):** يتحكم بإشارات المرور (12 دايود ضوئي)، ويحسب عدد السيارات في كل مسار باستخدام حساسات الأشعة تحت الحمراء (IR Sensors) عبر المقاطعات العتادية (Hardware Interrupts).
2. **الخوارزمية التكيفية (Adaptive Algorithm):** تحدد زمن الإشارة الخضراء ديناميكياً بناءً على كثافة السيارات (5 ثوانٍ، 15 ثانية، أو 25 ثانية).
3. **نظام الطوارئ (Emergency Override):** يسمح بفتح مسار فوري وآمن لسيارات الإسعاف والطوارئ لمدة 10 ثوانٍ مع تفريغ التقاطع عبر الإشارة الصفراء، ثم استئناف الدورة الطبيعية دون اضطراب.

---

## 🎯 متطلبات المشروع الأربعة (Core IoT Requirements)

| # | المتطلب (Requirement) | التقنية المستخدمة | الوصف |
|---|---|---|---|
| **1** | **MQTT Protocol** | Mosquitto Broker (Port 1883) | تواصل فوري وخفيف بين الـ ESP32 و Node-RED عبر قناتي `sanad/traffic/data` و `sanad/traffic/command`. |
| **2** | **Dashboard 2.0** | `@flowfuse/node-red-dashboard` | لوحة تحكم تفاعلية للمشغلين تحتوي على عدادات رقمية (Gauges) لأعداد السيارات وأزرار طوارئ سريعة. |
| **3** | **RESTful API** | Node-RED HTTP In/Response | توفير Endpoints برمجية (`GET /status` و `POST /emergency`) لاستهلاكها عبر Postman وتطبيق Flutter. |
| **4** | **Alerts System** | Telegram Bot & Gmail SMTP | إرسال تنبيهات فورية إلى هاتف المشرف وإيميل الإدارة عند تفعيل أي وضع طوارئ. |

---

## 📁 هيكلية المجلدات (Project Directory Structure)

```text
Traffic-Adaptive-IoT-Project/
│
├── 01_ESP32_Firmware/
│   ├── esp32_traffic_system/
│   │   └── esp32_traffic_system.ino    # كود الأردوينو النهائي (يدعم MQTT و ArduinoJson v7)
│   └── wokwi_simulation/               # ملفات المحاكاة على منصة Wokwi
│       ├── diagram.json
│       ├── libraries.txt
│       └── sketch.ino
│
├── 02_Node_RED/
│   ├── flows_traffic_system.json       # ملف التدفقات الكامل (Dashboard 2.0 + REST API + Alerts)
│   └── README_NodeRED.md               # دليل تثبيت وتشغيل Node-RED والنودات المطلوبة
│
├── 03_Mobile_App/
│   └── Flutter-app/                    # تطبيق الموبايل (مربوط بـ RESTful API)
│
├── 04_Documentation/
│   ├── Final_Project_Report_Template.docx # نموذج التوثيق المطلوب من أستاذ المادة
│   ├── Previous_Semester_Report.docx      # تقرير الفصل السابق (مرجع)
│   └── Previous_Semester_Report.pdf
│
└── README.md                           # هذا الملف التعريفي
```

---

## 🚀 دليل التشغيل السريع (Quick Start Guide)

### 1. تشغيل وسيط الـ MQTT (Mosquitto)
تأكد من تشغيل خدمة Mosquitto على لابتوبك:
```bash
net start mosquitto
```
أو تشغيله يدوياً عبر سطر الأوامر:
```bash
mosquitto -v
```

### 2. تشغيل Node-RED
1. افتح موجه الأوامر واكتب:
   ```bash
   node-red
   ```
2. ادخل على المتصفح: `http://localhost:1880`.
3. قم باستيراد (Import) ملف: `02_Node_RED/flows_traffic_system.json`.
4. اضغط **Deploy**.
5. ادخل على لوحة التحكم (Dashboard 2.0): `http://localhost:1880/dashboard`.

### 3. رفع الكود للـ ESP32
1. افتح ملف `01_ESP32_Firmware/esp32_traffic_system/esp32_traffic_system.ino` في Arduino IDE.
2. اضبط اسم شبكة الواي فاي وكلمة المرور، والـ IP الخاص بلابتوبك (`mqtt_server`).
3. اختر بورد `DOIT ESP32 DEVKIT V1` والمنفذ المناسب، واضغط **Upload**.

### 4. اختبار الـ RESTful API عبر Postman أو cURL
* **استعلام عن حالة التقاطع (GET):**
  ```bash
  curl http://localhost:1880/status
  ```
* **تفعيل طوارئ للشمال (POST):**
  ```bash
  curl -X POST http://localhost:1880/emergency -H "Content-Type: application/json" -d "{\"lane\": 1}"
  ```

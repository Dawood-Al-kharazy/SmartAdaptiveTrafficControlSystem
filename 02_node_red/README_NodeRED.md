# 📦 دليل تشغيل وتثبيت Node-RED (Dashboard 2.0 & APIs)

---

## 1. الحزم المطلوبة (Required Palette Nodes)
قبل استيراد ملف الـ Flow، تأكد من تثبيت الحزم التالية في Node-RED:

1. **Dashboard 2.0:**
   * الحزمة الرسمية: `@flowfuse/node-red-dashboard`
   * طريقة التثبيت: من القائمة العلوية في Node-RED $\rightarrow$ **Manage palette** $\rightarrow$ **Install** $\rightarrow$ ابحث عن `@flowfuse/node-red-dashboard` واضغط **Install**.
2. **بريد الإيميل (Email Node):**
   * الحزمة: `node-red-node-email`
   * عادةً تكون مثبتة تلقائياً مع Node-RED، وإذا لم تكن موجودة، ابحث عنها في Manage palette وثبتها.

---

## 2. كيفية استيراد الملف (How to Import Flow)
1. افتح Node-RED في المتصفح: `http://localhost:1880`.
2. من القائمة العلوية (أعلى اليمين ☰) اضغط **Import**.
3. اختر ملف: `flows_traffic_system.json`.
4. اضغط **Import** ثم اضغط الزر الأحمر الكبير **Deploy** في أعلى الشاشة.

---

## 3. الروابط المتاحة بعد التشغيل

| الرابط | الاستخدام |
|---|---|
| `http://localhost:1880` | بيئة تصميم التدفقات (Node-RED Editor) |
| `http://localhost:1880/dashboard` | لوحة التحكم التفاعلية (Dashboard 2.0) |
| `http://localhost:1880/status` | نقطة نهاية (REST API GET) لقراءة حالة الإشارة والسيارات |
| `http://localhost:1880/emergency` | نقطة نهاية (REST API POST) لتفعيل مسار طوارئ |

---

## 4. تجربة الـ RESTful API عبر Postman

### أ) قراءة الحالة (GET):
* **Method:** `GET`
* **URL:** `http://localhost:1880/status`
* **Response المتوقع:**
```json
{
  "nCars": 2,
  "eCars": 0,
  "sCars": 0,
  "wCars": 0,
  "state": 0
}
```

### ب) إرسال أمر طوارئ (POST):
* **Method:** `POST`
* **URL:** `http://localhost:1880/emergency`
* **Headers:** `Content-Type: application/json`
* **Body (raw JSON):**
```json
{
  "lane": 1
}
```
*(أرقام المسارات: 1=شمال North, 2=شرق East, 3=جنوب South, 4=غرب West)*

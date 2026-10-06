import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/traffic_state.dart';

class TrafficApiService {
  // للتشغيل على محاكي الأندرويد (Android Emulator):
  // 10.0.2.2 يشير تلقائياً إلى localhost في جهاز اللابتوب
  // إذا كنت تشغل التطبيق على هاتف حقيقي، غير الـ IP إلى عنوان لابتوبك مثل: http://192.168.1.100:1880
  static String baseUrl = 'http://192.168.43.100:1880';

  /// دفق بيانات دوري يطلب حالة المرور عبر RESTful API كل 800 مللي ثانية
  Stream<TrafficState> get trafficStream async* {
    yield TrafficState.initial();

    while (true) {
      yield await fetchTrafficStatus();
      await Future.delayed(const Duration(milliseconds: 800));
    }
  }

  /// طلب قراءة الحالة من السيرفر GET /status
  Future<TrafficState> fetchTrafficStatus() async {
    try {
      final response = await http
          .get(Uri.parse('$baseUrl/status'))
          .timeout(const Duration(seconds: 2));

      if (response.statusCode == 200) {
        final dynamic decoded = jsonDecode(response.body);
        if (decoded is Map) {
          return TrafficState.fromMap(Map<String, dynamic>.from(decoded));
        }
      }
    } catch (e) {
      // في حالة فشل الاتصال، نعيد آخر حالة معروفة أو الحالة الافتراضية
    }
    return TrafficState.initial();
  }

  /// إرسال أمر تفعيل الطوارئ POST /emergency
  Future<bool> triggerEmergency(int laneId) async {
    try {
      final response = await http.post(
        Uri.parse('$baseUrl/emergency'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'lane': laneId}),
      ).timeout(const Duration(seconds: 3));

      return response.statusCode == 200;
    } catch (e) {
      return false;
    }
  }
}

import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import '../models/traffic_state.dart';

class FirebaseService {
  final DatabaseReference _trafficRef;

  FirebaseService() : _trafficRef = FirebaseDatabase.instanceFor(
      app: Firebase.app(),
      databaseURL: 'https://adaptive-trafic-default-rtdb.europe-west1.firebasedatabase.app'
  ).ref('traffic');

  /// Stream of traffic state from Firebase
  Stream<TrafficState> get trafficStream async* {
    // Yield an initial state immediately so the screen doesn't get stuck loading
    yield TrafficState.initial();

    // Listen to Firebase and yield new states as they arrive
    yield* _trafficRef.onValue.map((event) {
      final data = event.snapshot.value;
      if (data != null && data is Map) {
        return TrafficState.fromMap(data);
      }
      return TrafficState.initial();
    });
  }

  /// Trigger emergency override for a specific lane (1: N, 2: E, 3: S, 4: W)
  Future<void> triggerEmergency(int laneId) async {
    try {
      await _trafficRef.update({
        'emergencyLane': laneId,
      });
    } catch (e) {
      print('Error triggering emergency: $e');
    }
  }

  /// Reset emergency override
  Future<void> resetEmergency() async {
    try {
      await _trafficRef.update({
        'emergencyLane': 0,
      });
    } catch (e) {
      print('Error resetting emergency: $e');
    }
  }
}

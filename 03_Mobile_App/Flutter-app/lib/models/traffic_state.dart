class TrafficState {
  final int nCars;
  final int eCars;
  final int sCars;
  final int wCars;
  final int state;
  final int emergencyLane;

  TrafficState({
    required this.nCars,
    required this.eCars,
    required this.sCars,
    required this.wCars,
    required this.state,
    required this.emergencyLane,
  });

  factory TrafficState.fromMap(Map<dynamic, dynamic> data) {
    return TrafficState(
      nCars: data['nCars'] ?? 0,
      eCars: data['eCars'] ?? 0,
      sCars: data['sCars'] ?? 0,
      wCars: data['wCars'] ?? 0,
      state: data['state'] ?? 0,
      emergencyLane: data['emergencyLane'] ?? 0,
    );
  }

  factory TrafficState.initial() {
    return TrafficState(
      nCars: 0,
      eCars: 0,
      sCars: 0,
      wCars: 0,
      state: 0,
      emergencyLane: 0,
    );
  }

  // Returns true if the light is yellow
  bool get isYellowState => state % 2 != 0;

  // Returns the active road index (0: North, 1: East, 2: South, 3: West)
  int get activeRoadIndex => state ~/ 2;

  List<int> get carCounts => [nCars, eCars, sCars, wCars];
}

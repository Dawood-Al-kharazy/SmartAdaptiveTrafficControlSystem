import 'dart:math';
import 'dart:ui';
import 'package:flutter/material.dart';
import '../../models/traffic_state.dart';
import '../../services/traffic_api_service.dart';
import '../widgets/road_details_widget.dart';
import '../widgets/control_button_widget.dart';

class TrafficControlScreen extends StatefulWidget {
  final VoidCallback onThemeToggle;
  const TrafficControlScreen({super.key, required this.onThemeToggle});

  @override
  State<TrafficControlScreen> createState() => _TrafficControlScreenState();
}

class _TrafficControlScreenState extends State<TrafficControlScreen> {
  final TrafficApiService _apiService = TrafficApiService();
  int? _optimisticEmergencyLane;

  Color _getStatusColor(TrafficState state, int index) {
    if (index == state.activeRoadIndex) {
      return state.isYellowState ? Colors.amberAccent : const Color(0xFF00FF7F);
    }
    return const Color(0xFFFF3B30);
  }

  void _triggerEmergency(int laneId) {
    // Optimistic UI Update: Instantly change the UI without waiting for network
    setState(() {
      _optimisticEmergencyLane = laneId;
    });
    
    // Send to Node-RED REST API in background
    _apiService.triggerEmergency(laneId).then((_) {
      if (mounted) {
        Future.delayed(const Duration(seconds: 1), () {
          if (mounted) {
            setState(() {
              _optimisticEmergencyLane = null;
            });
          }
        });
      }
    });
  }

  void _showServerIpDialog() {
    final controller = TextEditingController(text: TrafficApiService.baseUrl);
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text("Node-RED Server URL"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              "Emulator default: http://10.0.2.2:1880\nReal Phone: http://<Laptop-IP>:1880",
              style: TextStyle(fontSize: 12, color: Colors.grey),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              decoration: const InputDecoration(
                border: OutlineInputBorder(),
                labelText: "Base URL",
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text("Cancel"),
          ),
          ElevatedButton(
            onPressed: () {
              setState(() {
                TrafficApiService.baseUrl = controller.text.trim();
              });
              Navigator.pop(ctx);
            },
            child: const Text("Save"),
          ),
        ],
      ),
    );
  }

  Widget _buildGlassCard({required Widget child, required bool isDark, double? height}) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(24),
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
        child: Container(
          height: height,
          decoration: BoxDecoration(
            color: isDark ? Colors.white.withOpacity(0.05) : Colors.white.withOpacity(0.6),
            borderRadius: BorderRadius.circular(24),
            border: Border.all(
              color: isDark ? Colors.white.withOpacity(0.1) : Colors.white.withOpacity(0.8),
              width: 1,
            ),
          ),
          child: child,
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final bool isDark = Theme.of(context).brightness == Brightness.dark;
    const double roadThickness = 100.0;

    return Scaffold(
      extendBodyBehindAppBar: true,
      appBar: AppBar(
        title: const Text("Smart Traffic", style: TextStyle(fontWeight: FontWeight.w800, letterSpacing: 1.2)),
        centerTitle: true,
        backgroundColor: Colors.transparent,
        elevation: 0,
        leading: IconButton(
          onPressed: widget.onThemeToggle,
          icon: Icon(isDark ? Icons.wb_sunny_rounded : Icons.nightlight_round),
        ),
        actions: [
          IconButton(
            onPressed: _showServerIpDialog,
            icon: const Icon(Icons.settings_ethernet_rounded),
            tooltip: "Node-RED Server URL",
          ),
        ],
      ),
      body: Container(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: isDark
                ? [const Color(0xFF0D0D12), const Color(0xFF1A1A24)]
                : [const Color(0xFFF0F2F5), const Color(0xFFE2E6EE)],
          ),
        ),
        child: SafeArea(
          child: StreamBuilder<TrafficState>(
            stream: _apiService.trafficStream,
            builder: (context, snapshot) {
              if (snapshot.hasError) {
                return Center(child: Text('Error: ${snapshot.error}'));
              }
              if (!snapshot.hasData) {
                return const Center(child: CircularProgressIndicator());
              }

              final trafficState = snapshot.data!;
              final int totalCars = trafficState.nCars + trafficState.eCars + trafficState.sCars + trafficState.wCars;
              final bool isEmergency = (_optimisticEmergencyLane ?? trafficState.emergencyLane) != 0;
              final int activeEmergencyLane = _optimisticEmergencyLane ?? trafficState.emergencyLane;

              return Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16.0, vertical: 8.0),
                child: Column(
                  children: [
                    // --- Top Stats Panel ---
                    _buildGlassCard(
                      isDark: isDark,
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          children: [
                            Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text("TOTAL WAITING", style: TextStyle(fontSize: 10, fontWeight: FontWeight.bold, letterSpacing: 1.5, color: isDark ? Colors.grey[400] : Colors.grey[600])),
                                const SizedBox(height: 4),
                                Text("$totalCars Cars", style: const TextStyle(fontSize: 22, fontWeight: FontWeight.w900)),
                              ],
                            ),
                            Container(
                              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                              decoration: BoxDecoration(
                                color: isEmergency ? Colors.redAccent.withOpacity(0.2) : Colors.greenAccent.withOpacity(0.2),
                                borderRadius: BorderRadius.circular(20),
                                border: Border.all(color: isEmergency ? Colors.redAccent : Colors.greenAccent, width: 1.5),
                              ),
                              child: Row(
                                children: [
                                  Icon(
                                    isEmergency ? Icons.warning_rounded : Icons.check_circle_rounded,
                                    color: isEmergency ? Colors.redAccent : Colors.greenAccent,
                                    size: 16,
                                  ),
                                  const SizedBox(width: 6),
                                  Text(
                                    isEmergency ? "EMERGENCY" : "ADAPTIVE",
                                    style: TextStyle(
                                      color: isEmergency ? Colors.redAccent : Colors.greenAccent,
                                      fontSize: 12,
                                      fontWeight: FontWeight.bold,
                                      letterSpacing: 1.2,
                                    ),
                                  ),
                                ],
                              ),
                            )
                          ],
                        ),
                      ),
                    ),
                    const SizedBox(height: 20),

                    // --- Center Map ---
                    Expanded(
                      flex: 6,
                      child: _buildGlassCard(
                        isDark: isDark,
                        child: ClipRRect(
                          borderRadius: BorderRadius.circular(24),
                          child: LayoutBuilder(
                            builder: (context, constraints) {
                              double mapSize = min(constraints.maxWidth, constraints.maxHeight) - 20;
                              return Center(
                                child: SizedBox(
                                  width: mapSize,
                                  height: mapSize,
                                  child: Stack(
                                    alignment: Alignment.center,
                                    children: [
                                      // Road Base
                                      Container(
                                        width: roadThickness,
                                        height: mapSize,
                                        decoration: BoxDecoration(
                                          color: isDark ? const Color(0xFF1E1E24) : const Color(0xFFD0D4DB),
                                        ),
                                      ),
                                      Container(
                                        width: mapSize,
                                        height: roadThickness,
                                        decoration: BoxDecoration(
                                          color: isDark ? const Color(0xFF1E1E24) : const Color(0xFFD0D4DB),
                                        ),
                                      ),

                                      // Road Details
                                      Positioned(
                                        top: 0,
                                        bottom: mapSize / 2,
                                        child: RoadDetailsWidget(
                                          index: 0,
                                          label: "North",
                                          isVertical: true,
                                          isFirstHalf: true,
                                          width: roadThickness,
                                          totalLength: mapSize,
                                          statusColor: _getStatusColor(trafficState, 0),
                                          carCount: trafficState.nCars,
                                        ),
                                      ),
                                      Positioned(
                                        top: mapSize / 2,
                                        bottom: 0,
                                        child: RoadDetailsWidget(
                                          index: 2,
                                          label: "South",
                                          isVertical: true,
                                          isFirstHalf: false,
                                          width: roadThickness,
                                          totalLength: mapSize,
                                          statusColor: _getStatusColor(trafficState, 2),
                                          carCount: trafficState.sCars,
                                        ),
                                      ),
                                      Positioned(
                                        left: 0,
                                        right: mapSize / 2,
                                        child: RoadDetailsWidget(
                                          index: 3,
                                          label: "West",
                                          isVertical: false,
                                          isFirstHalf: true,
                                          width: roadThickness,
                                          totalLength: mapSize,
                                          statusColor: _getStatusColor(trafficState, 3),
                                          carCount: trafficState.wCars,
                                        ),
                                      ),
                                      Positioned(
                                        left: mapSize / 2,
                                        right: 0,
                                        child: RoadDetailsWidget(
                                          index: 1,
                                          label: "East",
                                          isVertical: false,
                                          isFirstHalf: false,
                                          width: roadThickness,
                                          totalLength: mapSize,
                                          statusColor: _getStatusColor(trafficState, 1),
                                          carCount: trafficState.eCars,
                                        ),
                                      ),

                                      // Central Intersection
                                      Container(
                                        width: roadThickness,
                                        height: roadThickness,
                                        decoration: BoxDecoration(
                                          color: isDark ? const Color(0xFF1E1E24) : const Color(0xFFD0D4DB),
                                          border: Border.all(color: isDark ? Colors.white12 : Colors.black12, width: 1),
                                          boxShadow: [
                                            BoxShadow(
                                              color: isDark ? Colors.black54 : Colors.transparent,
                                              blurRadius: 20,
                                            )
                                          ],
                                        ),
                                        child: Center(
                                          child: Icon(Icons.traffic_rounded, size: 40, color: isDark ? Colors.white38 : Colors.black38),
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              );
                            },
                          ),
                        ),
                      ),
                    ),

                    const SizedBox(height: 20),

                    // --- Bottom Controls ---
                    _buildGlassCard(
                      isDark: isDark,
                      height: 140,
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 15),
                        child: Column(
                          children: [
                            Text(
                              "EMERGENCY OVERRIDE",
                              style: TextStyle(
                                fontSize: 11,
                                fontWeight: FontWeight.w900,
                                letterSpacing: 1.5,
                                color: isDark ? Colors.white54 : Colors.black54,
                              ),
                            ),
                            const Spacer(),
                            Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              children: [
                                ControlButtonWidget(
                                  index: 0,
                                  label: "North",
                                  icon: Icons.arrow_upward_rounded,
                                  isActive: activeEmergencyLane == 1,
                                  onPressed: () => _triggerEmergency(1),
                                ),
                                ControlButtonWidget(
                                  index: 3,
                                  label: "West",
                                  icon: Icons.arrow_back_rounded,
                                  isActive: activeEmergencyLane == 4,
                                  onPressed: () => _triggerEmergency(4),
                                ),
                                ControlButtonWidget(
                                  index: 2,
                                  label: "South",
                                  icon: Icons.arrow_downward_rounded,
                                  isActive: activeEmergencyLane == 3,
                                  onPressed: () => _triggerEmergency(3),
                                ),
                                ControlButtonWidget(
                                  index: 1,
                                  label: "East",
                                  icon: Icons.arrow_forward_rounded,
                                  isActive: activeEmergencyLane == 2,
                                  onPressed: () => _triggerEmergency(2),
                                ),
                              ],
                            ),
                            const Spacer(),
                          ],
                        ),
                      ),
                    ),
                  ],
                ),
              );
            },
          ),
        ),
      ),
    );
  }
}

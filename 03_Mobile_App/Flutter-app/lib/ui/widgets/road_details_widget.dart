import 'package:flutter/material.dart';

class RoadDetailsWidget extends StatelessWidget {
  final int index;
  final String label;
  final bool isVertical;
  final bool isFirstHalf;
  final double width;
  final double totalLength;
  final Color statusColor;
  final int carCount;

  const RoadDetailsWidget({
    Key? key,
    required this.index,
    required this.label,
    required this.isVertical,
    required this.isFirstHalf,
    required this.width,
    required this.totalLength,
    required this.statusColor,
    required this.carCount,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    double segmentLength = totalLength / 2;

    return Container(
      width: isVertical ? width : segmentLength,
      height: isVertical ? segmentLength : width,
      child: Stack(
        children: [
          // Dashed lines
          Center(
            child: Flex(
              direction: isVertical ? Axis.vertical : Axis.horizontal,
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: List.generate(
                4,
                (i) => Container(
                  width: isVertical ? 2 : 15,
                  height: isVertical ? 15 : 2,
                  color: Colors.white24,
                ),
              ),
            ),
          ),

          // Stop line with Neon Glow
          Align(
            alignment: isVertical
                ? (isFirstHalf ? Alignment.bottomCenter : Alignment.topCenter)
                : (isFirstHalf ? Alignment.centerRight : Alignment.centerLeft),
            child: AnimatedContainer(
              duration: const Duration(milliseconds: 300),
              width: isVertical ? width : 6,
              height: isVertical ? 6 : width,
              decoration: BoxDecoration(
                color: statusColor,
                boxShadow: [
                  BoxShadow(
                    color: statusColor.withOpacity(0.8),
                    blurRadius: 15,
                    spreadRadius: 2,
                  ),
                ],
              ),
            ),
          ),

          // Neon Gradient Overlay
          AnimatedContainer(
            duration: const Duration(milliseconds: 300),
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: isVertical
                    ? (isFirstHalf ? Alignment.topCenter : Alignment.bottomCenter)
                    : (isFirstHalf ? Alignment.centerLeft : Alignment.centerRight),
                end: isVertical
                    ? (isFirstHalf ? Alignment.bottomCenter : Alignment.topCenter)
                    : (isFirstHalf ? Alignment.centerRight : Alignment.centerLeft),
                colors: [statusColor.withOpacity(0.0), statusColor.withOpacity(0.35)],
              ),
            ),
          ),

          // Animated Car indicator
          Align(
            alignment: Alignment.center,
            child: AnimatedContainer(
              duration: const Duration(milliseconds: 300),
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              decoration: BoxDecoration(
                color: Colors.black.withOpacity(0.7),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: statusColor.withOpacity(0.8), width: 1.5),
                boxShadow: [
                  BoxShadow(color: statusColor.withOpacity(0.5), blurRadius: 10, spreadRadius: 1)
                ],
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.directions_car_rounded, size: 14, color: statusColor),
                  const SizedBox(width: 6),
                  AnimatedSwitcher(
                    duration: const Duration(milliseconds: 300),
                    transitionBuilder: (Widget child, Animation<double> animation) {
                      return ScaleTransition(scale: animation, child: child);
                    },
                    child: Text(
                      "$carCount",
                      key: ValueKey<int>(carCount),
                      style: const TextStyle(
                        color: Colors.white,
                        fontSize: 14,
                        fontWeight: FontWeight.w900,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),

          // Direction Label
          Align(
            alignment: isVertical
                ? (isFirstHalf ? Alignment.topCenter : Alignment.bottomCenter)
                : (isFirstHalf ? Alignment.centerLeft : Alignment.centerRight),
            child: Padding(
              padding: const EdgeInsets.all(12.0),
              child: Text(
                label,
                style: const TextStyle(
                  color: Colors.white24,
                  fontWeight: FontWeight.w900,
                  fontSize: 14,
                  letterSpacing: 2,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

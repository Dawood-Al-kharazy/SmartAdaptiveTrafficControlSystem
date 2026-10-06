import 'package:flutter/material.dart';

class ControlButtonWidget extends StatelessWidget {
  final int index;
  final String label;
  final IconData icon;
  final bool isActive;
  final VoidCallback onPressed;

  const ControlButtonWidget({
    Key? key,
    required this.index,
    required this.label,
    required this.icon,
    required this.isActive,
    required this.onPressed,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final bool isDark = Theme.of(context).brightness == Brightness.dark;

    return GestureDetector(
      onTap: onPressed,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 300),
        width: 65,
        height: 65,
        decoration: BoxDecoration(
          color: isActive 
              ? Colors.redAccent.withOpacity(0.15) 
              : (isDark ? Colors.white.withOpacity(0.05) : Colors.black.withOpacity(0.05)),
          borderRadius: BorderRadius.circular(18),
          border: Border.all(
            color: isActive ? Colors.redAccent : (isDark ? Colors.white12 : Colors.black12),
            width: isActive ? 2 : 1,
          ),
          boxShadow: isActive
              ? [BoxShadow(color: Colors.redAccent.withOpacity(0.3), blurRadius: 15, spreadRadius: 2)]
              : [],
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            AnimatedSwitcher(
              duration: const Duration(milliseconds: 300),
              transitionBuilder: (Widget child, Animation<double> animation) {
                return ScaleTransition(scale: animation, child: child);
              },
              child: Icon(
                isActive ? Icons.warning_rounded : icon,
                key: ValueKey<bool>(isActive),
                color: isActive ? Colors.redAccent : (isDark ? Colors.white70 : Colors.black87),
                size: 24,
              ),
            ),
            const SizedBox(height: 6),
            Text(
              label,
              style: TextStyle(
                color: isActive ? Colors.redAccent : (isDark ? Colors.white54 : Colors.black54),
                fontSize: 10,
                fontWeight: FontWeight.w900,
                letterSpacing: 1.2,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

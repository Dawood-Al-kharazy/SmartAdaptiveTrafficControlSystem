import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:google_fonts/google_fonts.dart';
import 'ui/screens/traffic_control_screen.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const SmartTrafficSystemApp());
}

class SmartTrafficSystemApp extends StatefulWidget {
  const SmartTrafficSystemApp({super.key});

  @override
  State<SmartTrafficSystemApp> createState() => _SmartTrafficSystemAppState();
}

class _SmartTrafficSystemAppState extends State<SmartTrafficSystemApp> {
  ThemeMode _themeMode = ThemeMode.dark;
  bool _initialized = false;
  bool _error = false;

  @override
  void initState() {
    super.initState();
    _initializeFlutterFire();
  }

  Future<void> _initializeFlutterFire() async {
    try {
      if (Firebase.apps.isEmpty) {
        await Firebase.initializeApp();
      }
      setState(() {
        _initialized = true;
      });
    } catch (e) {
      print("Firebase Init Error: $e");
      setState(() {
        _error = true;
      });
    }
  }

  void _toggleTheme() {
    setState(() {
      _themeMode = _themeMode == ThemeMode.dark ? ThemeMode.light : ThemeMode.dark;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (_error) {
      return MaterialApp(
        home: Scaffold(
          body: Center(
            child: Text(
              'Failed to initialize Firebase.\nPlease check your configuration.',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.red, fontSize: 18),
            ),
          ),
        ),
      );
    }

    if (!_initialized) {
      return const MaterialApp(
        home: Scaffold(
          body: Center(
            child: CircularProgressIndicator(),
          ),
        ),
      );
    }

    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Smart Traffic',
      themeMode: _themeMode,
      theme: ThemeData.light().copyWith(
        scaffoldBackgroundColor: const Color(0xFFF0F2F5),
        textTheme: GoogleFonts.interTextTheme(ThemeData.light().textTheme),
        appBarTheme: const AppBarTheme(
          backgroundColor: Colors.white,
          foregroundColor: Colors.black87,
          elevation: 0,
        ),
      ),
      darkTheme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF0D0D12),
        textTheme: GoogleFonts.interTextTheme(ThemeData.dark().textTheme),
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFF0D0D12),
          foregroundColor: Colors.white,
          elevation: 0,
        ),
      ),
      home: TrafficControlScreen(onThemeToggle: _toggleTheme),
    );
  }
}

import 'package:battery_plus/battery_plus.dart';
import 'package:flutter/material.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final battery = Battery();

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: SizedBox.expand(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              FutureBuilder<int>(
                future: battery.batteryLevel,
                builder: (BuildContext context, AsyncSnapshot<int> snapshot) {
                  return Text(
                    '[${snapshot.hasData ? snapshot.data : "..."}]',
                    style: theme.textTheme.headline3,
                  );
                },
              ),
              StreamBuilder<BatteryState>(
                stream: battery.onBatteryStateChanged,
                builder: (BuildContext context, AsyncSnapshot<BatteryState> snapshot) {
                  return Text(
                    '[${snapshot.hasData ? snapshot.data : "..."}]',
                    style: theme.textTheme.headline3,
                  );
                },
              ),
            ],
          ),
        ),
      ),
    );
  }
}

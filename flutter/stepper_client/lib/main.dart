import 'package:flutter/material.dart';
import 'dart:ui';
//import 'dart:io';
import 'dart:async';
import 'package:http/http.dart' as http;
//import 'package:flutter_switch/flutter_switch.dart';
//import 'package:multicast_dns/multicast_dns.dart';
//import 'random.dart' as random;
import 'host.dart';
//import 'device.dart';

void main() {
  runApp(StepperApp());
}

class StepperApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Stepper Control',
      theme: ThemeData(
        brightness: Brightness.light,
        primarySwatch: Colors.red,
      ),
      darkTheme: ThemeData(
        brightness: Brightness.dark,
        primarySwatch: Colors.red,
      ),
      themeMode: ThemeMode.system,
      home: StepperHome(),
    );
  }
}

class StepperHome extends StatefulWidget {
  StepperHome({Key? key}) : super(key: key);

  @override
  _StepperHomeState createState() => _StepperHomeState();
}

class _StepperHomeState extends State<StepperHome> {
  late Hosts _hosts;
  List<String> _logList = <String>['Welcome.'];
  final int _logLimit = 100;
  late http.Client httpClient;

  @override
  void initState() {
    super.initState();
    httpClient = http.Client();
    _hosts = Hosts(setState, httpClient, log);
    _hosts.discover();
    new Timer.periodic(
      const Duration(seconds: 30),
      (Timer t) {
        _hosts.discover();
      },
    );
  }

  @override
  void dispose() {
    httpClient.close();
    super.dispose();
  }

  void log(String text) {
    setState(() {
      if (_logList.length >= _logLimit) {
        _logList.removeAt(_logList.length - 1);
      }
      _logList.insert(0, text);
    });
  }

  List<Widget> currentHostWidgetList() {
    //log('currentHostWidgetList');
    if (null == _hosts.current()) return <Widget>[Text("Discovering hosts...")];
    return _hosts.current()!.toWidgetList();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      /*
      appBar: AppBar(
        title: Text('Stepper Control'),
      ),
      */
      body: Column(
        mainAxisAlignment: MainAxisAlignment.start,
        mainAxisSize: MainAxisSize.max,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          Container(
            padding: EdgeInsets.fromLTRB(20, 40, 20, 10),
            transform: Matrix4.rotationZ(-0.02),
            child: Row(
              children: [
                _hosts.count() > 0
                    ? DropdownButton<String>(
                        value: _hosts.currentName(),
                        items: _hosts.toDropdownMenuItems(),
                        onChanged: (val) {
                          if (null != val)
                            setState(() {
                              _hosts.setCurrent(val);
                            });
                        },
                      )
                    : Text('Discovering...'),
              ],
            ),
          ),
          ...currentHostWidgetList(),
          Expanded(
            flex: 1,
            child: FractionallySizedBox(
              widthFactor: 1,
              child: Container(
                transform: Matrix4.rotationZ(0.02),
                padding: EdgeInsets.fromLTRB(20, 0, 20, 10),
                child: Scrollbar(
                  child: ListView.builder(
                    reverse: true,
                    padding: EdgeInsets.all(8),
                    itemCount: _logList.length,
                    itemBuilder: (BuildContext context, int index) {
                      return Container(
                        //height: 10,
                        child: Text(_logList[index]),
                      );
                    },
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

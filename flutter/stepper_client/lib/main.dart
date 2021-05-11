import 'package:flutter/material.dart';
import 'dart:ui';
//import 'dart:io';
import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:flutter_switch/flutter_switch.dart';
//import 'random.dart' as random;

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
  //String _host = "steppercontrol.local";
  String _host = '192.168.1.1';
  String _hostName = '';
  int _commandMin = -100;
  int _commandMax = 100;
  int _commandRate = 1000;
  int _vector = 0;
  bool _run = false;
  List<String> _logList = <String>['Welcome.'];
  final int _logLimit = 100;
  String _lastConfigFrom = '';
  int _lastCommandTime = 0;
  int _nextCommandTime = 0;
  late TextEditingController _hostController;
  late http.Client _client;

  @override
  void initState() {
    super.initState();
    new Timer.periodic(
      const Duration(milliseconds: 5000),
      (Timer t) {
        //_log(random.sentence(maxWords: 20));
      },
    );
    _hostController = TextEditingController(text: _host);
    _client = http.Client();
    //_client.connectionTimeout = const Duration(seconds: 10);
    _updateConfig();
  }

  @override
  void dispose() {
    _hostController.dispose();
    _client.close();
    super.dispose();
  }

  int _now() {
    return DateTime.now().millisecondsSinceEpoch;
  }

  void _sendCommand() {
    int now = _now();
    if ((now - _lastCommandTime) >= _commandRate) {
      _log("Sending  command: $_vector");
      _request('command', params: {'vector': _run ? _vector.toString() : '0'});
      _lastCommandTime = now;
    } else if (_nextCommandTime <= now) {
      _log("Delaying command: $_vector");
      _nextCommandTime = now + _commandRate;
      Future.delayed(Duration(milliseconds: _commandRate), _sendCommand);
    }
  }

  void _updateConfig() {
    _request('config').then(
      (json) {
        if (0 < json.length) {
          _lastConfigFrom = _host;
          _log("Config reply: $json");
          Map<String, dynamic> config = jsonDecode(json);
          _log(
              "Old config: $_hostName $_commandMin $_commandMax $_commandRate");
          _hostName = config['name'];
          _commandMin = config['min'];
          _commandMax = config['max'];
          _commandRate = config['rate'];
          _log(
              "New config: $_hostName $_commandMin $_commandMax $_commandRate");
          _log("Last config from: $_lastConfigFrom");
          _sendCommand();
        }
      },
      onError: (e) {
        _log("Config error: ${e.toString()}");
      },
    ).catchError(
      (e) {
        _log("Update config error: ${e.toString()}");
      },
    );
  }

  void _setHost(String value) {
    _log("Setting host to $value");
    setState(() {
      _host = value;
      _updateConfig();
    });
  }

  void _setVector(double value) {
    setState(() {
      _vector = value.toInt();
      _log("Vector is $_vector");
      _sendCommand();
    });
  }

  void _setRun(bool value) {
    setState(() {
      _run = value;
      _log("Setting run to $_run");
      _sendCommand();
    });
  }

  void _log(String text) {
    setState(() {
      if (_logList.length >= _logLimit) {
        _logList.removeAt(_logList.length - 1);
      }
      _logList.insert(0, text);
    });
  }

  Future<String> _request(String path, {Map<String, String>? params}) async {
    String responseBody = "";
    int statusCode = 0;

    try {
      var url = Uri.http(_host, path, params);
      //if (url.port == 0) {
      //url = url.replace(port: 80);
      //}
      _log("[HTTP] Url: ${url.toString()} Port: ${url.port.toString()}");
      var response = await _client.get(url).catchError((e) {
        _log("[HTTP AsyncAPI]: ${e.toString()}");
        throw Exception("Error: ${e.toString()}");
      });
      responseBody = response.body;
      statusCode = response.statusCode;
    } catch (e) {
      _log("[HTTP] ${e.toString()}");
    }
    if (statusCode == 200) {
      return responseBody;
    } else {
      _log(
          "[HTTP] Request to $path failed: ${statusCode.toString()}: $responseBody");
    }
    return "";
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
                Expanded(
                  child: TextField(
                    controller: _hostController,
                    onSubmitted: _setHost,
                    decoration: InputDecoration(
                      filled: true,
                      labelText: "Stepper Host:",
                    ),
                  ),
                ),
                /*
              ElevatedButton(
                onPressed: _toggleConnection,
                child: Text("Connect"),
              ),
              */
              ],
            ),
          ),
          Container(
            transform: Matrix4.rotationZ(0.05),
            child: Slider(
              key: null,
              onChanged: _setVector,
              value: _vector.toDouble(),
              min: _commandMin.toDouble(),
              max: _commandMax.toDouble(),
            ),
          ),
          Container(
            transform: Matrix4.rotationZ(-0.02),
            padding: EdgeInsets.fromLTRB(20, 10, 20, 20),
            child: Row(
              children: [
                Expanded(
                  child: Container(
                    transform: Matrix4.rotationZ(-0.5),
                    child: Text(
                      "$_vector",
                      style: TextStyle(
                        fontSize: 20.0,
                        fontWeight: FontWeight.w200,
                        fontFamily: "Roboto",
                      ),
                    ),
                  ),
                ),
                FlutterSwitch(
                  onToggle: _setRun,
                  value: _run,
                  showOnOff: true,
                  activeColor: Colors.red,
                ),
              ],
            ),
          ),
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

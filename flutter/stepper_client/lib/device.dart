import 'package:flutter/material.dart';
import 'package:flutter_switch/flutter_switch.dart';

abstract class Device {
  String name;
  String type;
  Function sendCommand;
  Function setState;

  Device(this.name, this.type, this.sendCommand, this.setState);

  factory Device.fromJson(
      Map<String, dynamic> json, Function sendCommand, Function setState) {
    var name = json['name'];
    if (null == name) throw Exception('Missing device name');
    var type = json['type'];
    if (null == type) throw Exception('Missing device type');
    switch (type) {
      case 'stepper':
        var stepper = Stepper(name, sendCommand, setState);
        stepper.configFromJson(json);
        return stepper;
      case 'led':
        return Led(name, sendCommand, setState);
    }
    throw Exception('Unknown device type');
  }

  List<Widget> toWidgetList();
}

class Stepper extends Device {
  int min = 0;
  int max = 0;
  int command = 0;
  bool enabled = false;

  Stepper(String name, Function sendCommand, Function setState)
      : super(name, 'stepper', sendCommand, setState);

  void configFromJson(Map<String, dynamic> json) {
    //debugPrint("configFromJson $name $json");
    if (json.containsKey('command_min')) min = json['command_min'].toInt();
    if (json.containsKey('command_max')) max = json['command_max'].toInt();
    command = (min + max) ~/ 2;
  }

  List<Widget> toWidgetList() {
    return <Widget>[
      Container(
        transform: Matrix4.rotationZ(0.05),
        child: Slider(
          key: null,
          onChanged: (value) {
            setState(() {
              command = value.toInt();
            });
            var commandToSend = enabled ? command : 0;
            sendCommand(name, {'command': commandToSend.toString()});
          },
          value: command.toDouble(),
          min: min.toDouble(),
          max: max.toDouble(),
        ),
      ),
      Container(
        transform: Matrix4.rotationZ(-0.02),
        padding: EdgeInsets.fromLTRB(20, 10, 20, 20),
        child: Row(
          children: [
            Expanded(
              child: Container(
                transform: Matrix4.rotationZ(command / 1000),
                child: Text(
                  command.toString(),
                  style: TextStyle(
                    fontSize: 20.0,
                    fontWeight: FontWeight.w200,
                    fontFamily: "Roboto",
                  ),
                ),
              ),
            ),
            FlutterSwitch(
              onToggle: (value) {
                setState(() {
                  enabled = value;
                });
                var commandToSend = enabled ? command : 0;
                sendCommand(name, {'command': commandToSend.toString()});
              },
              value: enabled,
              showOnOff: true,
              activeColor: Colors.red,
            ),
          ],
        ),
      ),
    ];
  }
}

class Led extends Device {
  bool enabled = false;

  Led(String name, Function sendCommand, Function setState)
      : super(name, 'led', sendCommand, setState);

  List<Widget> toWidgetList() {
    return <Widget>[
      Container(
        transform: Matrix4.rotationZ(0.04),
        padding: EdgeInsets.fromLTRB(20, 10, 20, 20),
        child: Row(
          children: [
            Expanded(
              child: Container(
                transform: Matrix4.rotationZ(-0.15),
                child: Text(
                  enabled ? "🌝" : "🌚",
                  style: TextStyle(
                    fontSize: 20.0,
                    fontWeight: FontWeight.w200,
                    fontFamily: "Roboto Mono",
                  ),
                ),
              ),
            ),
            FlutterSwitch(
              onToggle: (value) {
                setState(() {
                  enabled = value;
                });
                sendCommand(name, {'enable': enabled.toString()});
              },
              value: enabled,
              showOnOff: true,
              activeColor: Colors.red,
            ),
          ],
        ),
      ),
    ];
  }
}

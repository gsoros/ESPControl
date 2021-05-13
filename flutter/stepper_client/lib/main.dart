import 'package:flutter/material.dart';
import 'dart:ui';
//import 'dart:io';
import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:flutter_switch/flutter_switch.dart';
import 'package:multicast_dns/multicast_dns.dart';
//import 'random.dart' as random;

void main() {
  runApp(StepperApp());
}

class Host {
  String name;
  String ip;
  Map<String, int> services;
  String _currentService = '';

  Host(this.name, this.ip, this.services);

  replace(Host host) {
    name = host.name;
    ip = host.ip;
    services = host.services;
  }

  update(Host host) {
    name = host.name;
    ip = host.ip;
    host.services.forEach((service, port) {
      services[service] = port;
    });
  }

  String currentService() {
    if (hasService(_currentService)) {
      return _currentService;
    }
    return services.isEmpty ? '' : services.keys.first;
  }

  setCurrentService(String service) {
    if (hasService(service)) {
      _currentService = service;
    }
  }

  bool hasService(String service) {
    return services.containsKey(service);
  }

  Map<String, dynamic> toJson() => {
        'name': name,
        'ip': ip,
        'services': services,
      };

  String formatted(String service) => '$name:$service';

  DropdownMenuItem<String> toDropdownMenuItem(String service) {
    String formatted = this.formatted(service);
    return DropdownMenuItem(
      child: Text(formatted),
      value: formatted,
    );
  }

  List<DropdownMenuItem<String>> toDropdownMenuItems() {
    List<DropdownMenuItem<String>> items = [];
    services.forEach((service, port) {
      items.add(this.toDropdownMenuItem(service));
    });
    return items;
  }
}

class Hosts {
  Map<String, Host> hosts = {};
  String _current = '';

  init() {
    Host host = defualt();
    hosts = <String, Host>{host.name: host};
    _current = host.name;
  }

  Host defualt() {
    return Host(
      'discovering',
      '192.168.1.1',
      <String, int>{' services...': 80},
    );
  }

  updateHost(Host host) {
    if (has(host.name)) {
      hosts[host.name]!.update(host);
    } else {
      hosts[host.name] = host;
    }
  }

  update(Hosts newHosts) {
    String service = currentService();
    hosts = newHosts.hosts;
    setCurrentService(service);
  }

  Host current() {
    if (has(_current)) {
      return get(_current)!;
    }
    if (hosts.isEmpty) {
      return defualt();
    }
    return hosts[hosts.keys.first]!;
  }

  setCurrent(String? formatted) {
    if (formatted == null) return;
    String hostName = formatted.substring(0, formatted.indexOf(':'));
    String service = formatted.substring(formatted.indexOf(':') + 1);
    if (has(hostName)) {
      _current = hostName;
    }
    setCurrentService(service);
  }

  String currentService() {
    return current().currentService();
  }

  setCurrentService(String service) {
    current().setCurrentService(service);
  }

  bool has(String hostName) {
    return hosts.containsKey(hostName);
  }

  Host? get(String hostName) {
    return has(hostName) ? hosts[hostName] : null;
  }

  Map<String, dynamic> toJson() {
    Map<String, dynamic> json = {};
    hosts.forEach((name, host) {
      json[name] = host.toJson();
    });
    return json;
  }

  List<DropdownMenuItem<String>> toDropdownMenuItems() {
    List<DropdownMenuItem<String>> items = [];
    hosts.forEach((name, host) {
      items.addAll(host.toDropdownMenuItems());
    });
    return items;
  }
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
  Hosts _hosts = Hosts();
  int _commandMin = -100;
  int _commandMax = 100;
  int _commandRate = 1000;
  int _vector = 0;
  bool _run = false;
  List<String> _logList = <String>['Welcome.'];
  final int _logLimit = 100;
  int _lastCommandTime = 0;
  int _nextCommandTime = 0;
  late http.Client _client;
  String _mDNSServiceType = '_http._tcp';
  //String _mDNSServiceType = '_steppercontrol._tcp';

  @override
  void initState() {
    super.initState();
    _hosts.init();
    new Timer.periodic(
      const Duration(seconds: 5),
      (Timer t) {
        //_log(random.sentence(maxWords: 20));
        //_log('Starting mDNS discovery');
        _discoverHosts();
      },
    );
    _client = http.Client();
  }

  @override
  void dispose() {
    _client.close();
    super.dispose();
  }

  void _sendCommand() {
    int now = DateTime.now().millisecondsSinceEpoch;
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
          _log("Config reply: $json");
          Map<String, dynamic> config = jsonDecode(json);
          _log("Old config: $_commandMin $_commandMax $_commandRate");
          setState(() {
            _commandMin = config['min'];
            _commandMax = config['max'];
            _commandRate = config['rate'];
          });
          _log("New config: $_commandMin $_commandMax $_commandRate");
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
      var host = _hosts.current();
      var name = host.name;
      var ip = host.ip;
      var service = _hosts.currentService();
      var url = Uri.http(ip, path, params);
      if (!host.services.containsKey(service)) {
        throw Exception('Port not found for $name:$service');
      }
      url = url.replace(port: host.services[service]);
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

  void _discoverHosts() async {
    Hosts discovered = Hosts();

    final MDnsClient client = MDnsClient();
    await client.start();

    await for (final PtrResourceRecord ptr in client.lookup<PtrResourceRecord>(
        ResourceRecordQuery.serverPointer(_mDNSServiceType))) {
      //_log('[mDNS] PTR: ${ptr.toString()}');

      await for (final SrvResourceRecord srv
          in client.lookup<SrvResourceRecord>(
              ResourceRecordQuery.service(ptr.domainName))) {
        //_log('[mDNS] SRV target: ${srv.target} port: ${srv.port}');

        await for (final IPAddressResourceRecord ip
            in client.lookup<IPAddressResourceRecord>(
                ResourceRecordQuery.addressIPv4(srv.target))) {
          //_log('[mDNS] IP: ${ip.address.toString()}');
          String serviceName =
              ptr.domainName.substring(0, ptr.domainName.indexOf('.'));
          discovered.updateHost(Host(srv.target, ip.address.address.toString(),
              <String, int>{serviceName: srv.port}));
        }
      }
    }

    client.stop();
    _log('[mDNS] Discovered: ${discovered.toJson()}');
    // if (current host is gone or (current host available but does not have current service))
    var current = _hosts.current().name;
    bool gone = !discovered.has(current);
    bool serviceGone = !gone &&
        !discovered.hosts[current]!.hasService(_hosts.currentService());
    _log('gone? $gone, serviceGone? $serviceGone');
    setState(() {
      _hosts.update(discovered);
    });
    if (gone || serviceGone) {
      _log('triggering updateConfig()');
      _updateConfig();
    }
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
                DropdownButton<String>(
                  value: _hosts.current().formatted(_hosts.currentService()),
                  items: _hosts.toDropdownMenuItems(),
                  onChanged: (val) {
                    setState(() {
                      _hosts.setCurrent(val);
                      _updateConfig();
                    });
                  },
                ),
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

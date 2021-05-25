import 'package:multicast_dns/multicast_dns.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import 'package:flutter/material.dart';
import 'device.dart';

class Host {
  String name;
  String ip;
  int port;
  int rate = 100;
  http.Client httpClient;
  Function setState;
  Function log;
  Map<String, Device> devices = {};
  int _lastCommandTime = 0;
  int _nextCommandTime = 0;
  Map<String, Map<String, String>> _commands = {};

  Host(this.name, this.ip, this.port, this.httpClient, this.setState, this.log);

  void replace(Host host) {
    name = host.name;
    ip = host.ip;
    port = host.port;
    devices = host.devices;
  }

  void update(Host host) {
    name = host.name;
    ip = host.ip;
    port = host.port;
    host.devices.forEach((deviceName, device) {
      updateDevice(device);
    });
  }

  void updateDevice(Device device) {
    if (devices.containsKey(device.name))
      log("Updated device ${device.name}");
    else
      log("New device ${device.name}");
    devices[device.name] = device;
  }

  bool hasDevice(String device) {
    return devices.containsKey(device);
  }

  Map<String, dynamic> toJson() => {
        'name': name,
        'ip': ip,
        'port': port,
        'devices': devices,
      };

  String formatted() => name;

  DropdownMenuItem<String> toDropdownMenuItem() {
    return DropdownMenuItem(
      child: Text(name),
      value: name,
    );
  }

  Future<String> request(String path, {Map<String, String>? params}) async {
    String responseBody = "";
    int statusCode = 0;

    try {
      var url = Uri.http(ip, path, params);
      url = url.replace(port: port);
      debugPrint("[HTTP] Url: ${url.toString()} Port: ${url.port.toString()}");
      var response = await httpClient.get(url).catchError((e) {
        //debugPrint("[HTTP AsyncAPI]: ${e.toString()}");
        throw Exception("Error: ${e.toString()}");
      });
      responseBody = response.body;
      statusCode = response.statusCode;
    } catch (e) {
      //debugPrint("[HTTP] ${e.toString()}");
    }
    if (statusCode == 200) {
      debugPrint("[HTTP] Request to $path OK, response: $responseBody");
      return responseBody;
    } else {
      var msg =
          "[HTTP] Request to $path failed: ${statusCode.toString()} $responseBody";
      debugPrint(msg);
      log(msg);
    }
    return "";
  }

  void updateConfig() {
    request('/api/config').then(
      (json) {
        if (0 < json.length) {
          debugPrint("Config reply from $name: $json");
          Map<String, dynamic> config = jsonDecode(json);

          if (null != config['rate'] && rate != config['rate'])
            rate = config['rate'];
          if (null != config['devices']) {
            config['devices'].forEach((jsonDevice) {
              if (jsonDevice.containsKey('name') &&
                  !hasDevice(jsonDevice['name'])) {
                debugPrint("[Config] Host: $name  Device: $jsonDevice");
                setState(() {
                  updateDevice(
                      Device.fromJson(jsonDevice, sendCommand, setState));
                });
              }
            });
          }
        }
      },
      onError: (e) {
        debugPrint("Config error from $name: ${e.toString()}");
      },
    ).catchError(
      (e) {
        debugPrint("Update config error from $name: ${e.toString()}");
      },
    );
  }

  void sendCommand(String device, [Map<String, String>? command]) {
    //debugPrint("sendCommand $name:$device@$rate $command");
    if (device.length <= 0 || !hasDevice(device)) return;
    if (null == command || 0 == command.length) {
      if (_commands.containsKey(device)) {
        command = _commands[device];
      } else
        return;
    } else
      _commands[device] = command;
    if (null == command) return;

    int now = DateTime.now().millisecondsSinceEpoch;
    if ((now - _lastCommandTime) >= rate) {
      debugPrint("Sending command to $device: $command");
      request('/api/control', params: {
        ...{'device': device},
        ...command
      });
      _lastCommandTime = now;
    } else if (_nextCommandTime <= now) {
      debugPrint("Delaying command: $command");
      _nextCommandTime = now + rate;
      Future.delayed(Duration(milliseconds: rate), () => sendCommand(device));
    }
  }

  List<Widget> toWidgetList() {
    List<Widget> widgets = [];
    devices.forEach((name, device) {
      widgets.add(
        Container(
          color: Colors.black12,
          margin: EdgeInsets.fromLTRB(4, 4, 4, 4),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                padding: EdgeInsets.fromLTRB(20, 0, 0, 0),
                child: Text("$name"),
              ),
              ...device.toWidgetList(),
            ],
          ),
        ),
      );
    });
    if (widgets.length < 1)
      widgets.add(Text("This host has no devices configured"));
    //debugPrint("$name.toWidgetList(): $widgets");
    widgets.add(SizedBox(height: 50));
    return widgets;
  }
}

class Hosts {
  Map<String, Host> hosts = {};
  String _current = '';
  Function setState;
  http.Client httpClient;
  Function log;
  //String _mDNSServiceType = '_http._tcp';
  String _mDNSServiceType = '_steppercontrol._tcp';

  Hosts(this.setState, this.httpClient, this.log);

  int count() {
    return hosts.length;
  }

  void updateHost(Host host) {
    if (has(host.name)) {
      hosts[host.name]!.update(host);
    } else {
      hosts[host.name] = host;
    }
  }

  void update(Hosts newHosts) {
    hosts = newHosts.hosts;
  }

  void remove(String hostName) {
    if (has(hostName)) {
      debugPrint("Removing $hostName");
      hosts.remove(hostName);
      checkCurrent();
    }
  }

  void removeWhere(bool test(String name, Host host)) {
    hosts.removeWhere(test);
    checkCurrent();
  }

  void checkCurrent() {
    if (!hosts.keys.contains(_current)) {
      if (hosts.length > 0)
        setCurrent(hosts.keys.first);
      else {
        setCurrent('');
      }
    }
  }

  Host? current() {
    if (has(_current)) {
      return get(_current)!;
    }
    if (hosts.isEmpty) {
      return null;
    }
    return hosts[hosts.keys.first]!;
  }

  void setCurrent(String hostName) {
    debugPrint("Setting current to $hostName");
    if (!has(hostName)) {
      debugPrint("[Warning] Current is invalid");
    }
    if (_current != hostName) {
      setState(() {
        _current = hostName;
      });
      current()?.updateConfig();
    }
  }

  bool has(String hostName) {
    //debugPrint("has: $hostName ${hosts.keys}");
    return hosts.containsKey(hostName);
  }

  Host? get(String hostName) {
    return has(hostName) ? hosts[hostName] : null;
  }

  bool changed(Host newHost) {
    var host = get(newHost.name);
    if (null == host) {
      debugPrint("changed: new ${newHost.name}");
      return true;
    }
    if (host.name != newHost.name ||
        host.ip != newHost.ip ||
        host.port != newHost.port) {
      debugPrint("changed: params changed\n" +
          "${host.name}:${host.ip}:${host.port}\n" +
          "${newHost.name}:${newHost.ip}:${newHost.port}");
      return true;
    }
    return false;
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
      items.add(host.toDropdownMenuItem());
      //debugPrint('dropdown: ${host.name}');
    });
    return items;
  }

  String currentName() {
    Host? current = this.current();
    if (null == current) return 'Host not available';
    debugPrint('currentName: ${current.name}');
    return current.name;
  }

  Iterable<String> names() {
    return hosts.keys;
  }

  void discover() async {
    List<String> discovered = [];
    final MDnsClient client = MDnsClient();
    //debugPrint('[mDNS] Strarting discovery');
    await client.start();

    await for (final PtrResourceRecord ptr in client.lookup<PtrResourceRecord>(
        ResourceRecordQuery.serverPointer(_mDNSServiceType))) {
      //debugPrint('[mDNS] PTR: ${ptr.toString()}');

      await for (final SrvResourceRecord srv
          in client.lookup<SrvResourceRecord>(
              ResourceRecordQuery.service(ptr.domainName))) {
        //debugPrint('[mDNS] SRV target: ${srv.target} port: ${srv.port}');

        await for (final IPAddressResourceRecord ip
            in client.lookup<IPAddressResourceRecord>(
                ResourceRecordQuery.addressIPv4(srv.target))) {
          //debugPrint('[mDNS] IP: ${ip.address.toString()}');
          String name =
              ptr.domainName.substring(0, ptr.domainName.indexOf('.'));
          //debugPrint('[mDNS] Name: $name');

          if (name.length > 0) {
            //debugPrint("Checking $name");
            if (!discovered.contains(name)) discovered.add(name);
            var host = Host(name, ip.address.address.toString(), srv.port,
                httpClient, setState, log);
            if (changed(host)) {
              if (!has(name))
                log("Host $name discovered");
              else
                log("Host $name changed");
              setState(() {
                updateHost(host);
              });
              if (currentName() == name) {
                debugPrint('triggering updateConfig()');
                current()?.updateConfig();
              }
            }
          }
        }
      }
    }
    debugPrint("Discovered: $discovered");
    List<String> toRemove = [];
    names().forEach((name) {
      if (!discovered.contains(name)) {
        toRemove.add(name);
      }
    });
    if (toRemove.length > 0) {
      debugPrint("Removing: $toRemove");
      log("Disappeared: ${toRemove.join(", ")}");
      setState(() {
        removeWhere((name, host) => toRemove.contains(name));
      });
    }
    client.stop();
  }
}

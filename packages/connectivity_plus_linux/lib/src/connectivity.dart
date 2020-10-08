import 'dart:async';

import 'package:connectivity_plus_platform_interface/connectivity_plus_platform_interface.dart';
import 'package:meta/meta.dart';

import 'network_manager.dart';

typedef _DeviceGetter = Future<String> Function(NMDevice device);
typedef _ConnectionGetter = Future<String> Function(NMConnection connection);

@visibleForTesting
typedef NetworkManagerFactory = NetworkManager Function();

class ConnectivityLinux extends ConnectivityPlatform {
  @override
  Future<ConnectivityResult> checkConnectivity() {
    final manager = createManager();
    return _getConnectivity(manager).whenComplete(() => manager.dispose());
  }

  @override
  Future<String> getWifiName() {
    return _getConnectionValue((connection) => connection.getId());
  }

  @override
  Future<String> getWifiIP() {
    return _getDeviceValue((device) => device.getIp4());
  }

  @override
  Future<String> getWifiBSSID() {
    return _getDeviceValue((device) {
      return device
          .asWirelessDevice()
          .then((wireless) => wireless?.getHwAddress());
    });
  }

  Future<String> _getDeviceValue(_DeviceGetter getter) {
    return _getConnectionValue((connection) {
      return connection.createDevice().then((device) {
        return device != null ? getter(device) : null;
      });
    });
  }

  Future<String> _getConnectionValue(_ConnectionGetter getter) {
    final manager = createManager();
    return manager.createConnection().then((connection) {
      return connection != null ? getter(connection) : null;
    }).whenComplete(() => manager.dispose());
  }

  NetworkManager _connectivityManager;
  StreamController<ConnectivityResult> _connectivityController;

  @override
  Stream<ConnectivityResult> get onConnectivityChanged {
    _connectivityController ??= StreamController<ConnectivityResult>.broadcast(
      onListen: _startListenConnectivity,
      onCancel: _stopListenConnectivity,
    );
    return _connectivityController.stream;
  }

  Future<ConnectivityResult> _getConnectivity(NetworkManager manager) {
    return manager.getType().then((value) => value.toConnectivityResult());
  }

  void _startListenConnectivity() {
    _connectivityManager ??= createManager();
    _connectivityManager.getType().then((type) => _addConnectivity(type));
    _connectivityManager.subscribeTypeChanged().listen((type) {
      _addConnectivity(type);
    });
  }

  void _addConnectivity(String type) {
    _connectivityController.add(type.toConnectivityResult());
  }

  void _stopListenConnectivity() {
    _connectivityManager?.dispose();
    _connectivityManager = null;
  }

  @visibleForTesting
  NetworkManagerFactory createManager = () => NetworkManager.system();
}

extension _NMConnectivityType on String {
  ConnectivityResult toConnectivityResult() {
    if (isEmpty) {
      return ConnectivityResult.none;
    }
    if (contains('wireless')) {
      return ConnectivityResult.wifi;
    }
    // ### TODO: ethernet
    //if (contains('ethernet')) {
    //  return ConnectivityResult.ethernet;
    //}
    // gsm, cdma, bluetooth, ...
    return ConnectivityResult.mobile;
  }
}

extension _NMConnectivityResult on NMDeviceType {
  ConnectivityResult toConnectivityResult() {
    switch (this) {
      case NMDeviceType.wifi:
        return ConnectivityResult.wifi;
      // ### TODO: ethernet
      //case NMDeviceType.ethernet:
      //  return ConnectivityResult.ethernet;
      default:
        return ConnectivityResult.mobile;
    }
  }
}

#include "include/battery_plus_windows/battery_plus_windows_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>
#include <iostream>

namespace {

  class BatteryPlusWindowsPlugin : public flutter::Plugin {
    public:
      static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

      BatteryPlusWindowsPlugin();

      virtual ~BatteryPlusWindowsPlugin();

    private:
      // Called when a method is called on this plugin's channel from Dart.
      void HandleMethodCall(
        const flutter::MethodCall <flutter::EncodableValue> &method_call,
        std::unique_ptr <flutter::MethodResult<flutter::EncodableValue>> result);

      void HandleOnListen(std::unique_ptr <flutter::EventSink<flutter::EncodableValue>> &&events);

      bool HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

      void ReportChargingState();

      void ListenForEvents();

      void HandleOnCancel();

      std::unique_ptr <flutter::EventSink<flutter::EncodableValue>> eventSink;
      ATOM aWndClass = 0;
      HANDLE hThread = NULL;
      DWORD dwThreadId = 0;
      HPOWERNOTIFY hPowerHandle = NULL;

      static DWORD WINAPI ThreadProc(LPVOID lpParameter){
        BatteryPlusWindowsPlugin *plugin = reinterpret_cast<BatteryPlusWindowsPlugin *>(lpParameter);
        plugin->ListenForEvents();
        return 0;
      }

      static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
        BatteryPlusWindowsPlugin *plugin = reinterpret_cast<BatteryPlusWindowsPlugin *>(
          GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if (plugin->HandleWindowMessage(hWnd, uMsg, wParam, lParam)) {
          return 0;
        } else {
          return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
      }
  };

// static
  void BatteryPlusWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
    auto plugin = std::make_unique<BatteryPlusWindowsPlugin>();

    auto methodChannel =
      std::make_unique < flutter::MethodChannel < flutter::EncodableValue >> (
        registrar->messenger(), "plugins.flutter.io/battery",
          &flutter::StandardMethodCodec::GetInstance());

    auto eventChannel =
      std::make_unique < flutter::EventChannel < flutter::EncodableValue >> (
        registrar->messenger(), "plugins.flutter.io/charging",
          &flutter::StandardMethodCodec::GetInstance());

    methodChannel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

    auto streamHandler =
      std::make_unique < flutter::StreamHandlerFunctions < flutter::EncodableValue >> (
        [plugin_pointer = plugin.get()](const flutter::EncodableValue *arguments,
                                        std::unique_ptr <flutter::EventSink<flutter::EncodableValue>> &&events)
          -> std::unique_ptr <flutter::StreamHandlerError<flutter::EncodableValue>> {
          plugin_pointer->HandleOnListen(std::move(events));
          return nullptr;
        },
          [plugin_pointer = plugin.get()](const flutter::EncodableValue *arguments)
            -> std::unique_ptr <flutter::StreamHandlerError<flutter::EncodableValue>> {
            plugin_pointer->HandleOnCancel();
            return nullptr;
          });

    eventChannel->SetStreamHandler(std::move(streamHandler));

    registrar->AddPlugin(std::move(plugin));
  }

  BatteryPlusWindowsPlugin::BatteryPlusWindowsPlugin() {
    std::cout << "BatteryPlusWindowsPlugin Constructed";
  }

  BatteryPlusWindowsPlugin::~BatteryPlusWindowsPlugin() {
    std::cout << "BatteryPlusWindowsPlugin Deconstructed";
  }

  void BatteryPlusWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall <flutter::EncodableValue> &method_call,
    std::unique_ptr <flutter::MethodResult<flutter::EncodableValue>> result) {
    std::cout << "BatteryPlusWindowsPlugin methodCall: " << method_call.method_name();
    if (method_call.method_name().compare("getBatteryLevel") == 0) {
      SYSTEM_POWER_STATUS status;
      GetSystemPowerStatus(&status);
      int percent = status.BatteryLifePercent;
      if(percent == 255) {
        // unknown percentage / no-battery
        percent = 100;
      }
      flutter::EncodableValue response(percent);
      result->Success(&response);
    } else {
      result->NotImplemented();
    }
  }

  void BatteryPlusWindowsPlugin::HandleOnListen(
    std::unique_ptr <flutter::EventSink<flutter::EncodableValue>> &&events) {
    std::cout << "BatteryPlusWindowsPlugin onListen";
    eventSink = std::move(events);
    ReportChargingState();
    hThread = CreateThread(NULL, 0,
                           BatteryPlusWindowsPlugin::ThreadProc, this,
                           0, &dwThreadId);
  }

  void BatteryPlusWindowsPlugin::ListenForEvents() {
    IsGUIThread(TRUE);
    if (aWndClass == 0) {
      WNDCLASS wndClass = {
        0,
        BatteryPlusWindowsPlugin::WindowProc,
        0, // cbClsExtra
        0, // cbWndExtra
        GetModuleHandle(NULL),
        NULL,
        NULL,
        NULL,
        NULL,
        TEXT("BatteryPlusPluginWindow"),
      };
      aWndClass = RegisterClass(&wndClass);
    }
    HWND hWnd = CreateWindowEx(0, MAKEINTATOM(aWndClass), TEXT(""), 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    DestroyWindow(hWnd);
  }

  bool BatteryPlusWindowsPlugin::HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if(uMsg == WM_POWERBROADCAST && wParam == PBT_APMPOWERSTATUSCHANGE) {
      ReportChargingState();
      return true;
    }
    return false;
  }

  void BatteryPlusWindowsPlugin::ReportChargingState() {
    SYSTEM_POWER_STATUS status;
    if(!GetSystemPowerStatus(&status)){
      return;
    }
    std::ostringstream state;
    if(status.BatteryLifePercent == 100) {
      state << "full";
    }
    else if((status.ACLineStatus & 0x01)){
      if((status.BatteryFlag & 0x08)){
        state << "charging";
      }
      else{
        state << "full";
      }
    }else{
      state << "discharging";
    }
    std::cerr << "Value changed " << state.str();
    flutter::EncodableValue response(state.str());
    eventSink->Success(&response);
  }

  void BatteryPlusWindowsPlugin::HandleOnCancel() {
    if (hThread != NULL) {
      PostThreadMessage(dwThreadId, WM_QUIT, 0, 0);
      WaitForSingleObject(hThread, 100);
      CloseHandle(hThread);
      dwThreadId = 0;
      hThread = NULL;
    }
    std::cout << "BatteryPlusWindowsPlugin onCancel";
  }

}  // namespace

void BatteryPlusWindowsPluginRegisterWithRegistrar(
  FlutterDesktopPluginRegistrarRef registrar) {
  BatteryPlusWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarManager::GetInstance()
      ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

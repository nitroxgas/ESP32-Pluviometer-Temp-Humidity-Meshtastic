#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

// Estrutura para armazenar as configurações
struct WeatherStationConfig {
  uint8_t deepSleepTimeMinutes;
  uint16_t cpuFreqMHz;
  float rainMmPerTip;
  char wifiSsid[32];
  char wifiPassword[64];
  char meshtasticNodeIP[16];
  uint16_t meshtasticNodePort;
  char deviceName[32];
  bool configValid;
};

class ConfigManager {
public:
  ConfigManager();
  
  // Inicializa o gestor de configuração
  bool begin();
  
  // Gerenciamento de configurações
  bool loadConfig();
  bool saveConfig();
  void resetToDefaults();
  
  // BLE
  void startBLEServer();
  void stopBLEServer();
  bool isBLEConnected();
  
  // Web Server
  void startConfigPortal();
  void stopConfigPortal();
  bool isPortalActive();
  void handlePortal();
  
  // Acesso às configurações
  WeatherStationConfig* getConfig();
  
  // Verifica se o botão de configuração foi pressionado
  bool checkConfigButtonPressed();

private:
  WeatherStationConfig _config;
  bool _isPortalActive;
  bool _isBLEActive;
  unsigned long _portalStartTime;
  unsigned long _lastButtonCheckTime;
  bool _lastButtonState;
  
  // Instâncias BLE
  NimBLEServer* _pServer;
  NimBLEService* _pService;
  NimBLECharacteristic* _pCharacteristic;
  bool _bleConnected;
  
  // Web Server
  AsyncWebServer* _webServer;
  
  // Gerenciamento de arquivos
  String readFile(const char* path);
  bool writeFile(const char* path, const char* message);
  
  // Callbacks BLE
  class ServerCallbacks : public NimBLEServerCallbacks {
  public:
    ServerCallbacks(ConfigManager* configManager) : _configManager(configManager) {}
    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;
  private:
    ConfigManager* _configManager;
  };
  
  class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  public:
    CharacteristicCallbacks(ConfigManager* configManager) : _configManager(configManager) {}
    void onWrite(NimBLECharacteristic* pCharacteristic) override;
    void onRead(NimBLECharacteristic* pCharacteristic) override;
  private:
    ConfigManager* _configManager;
  };
  
  ServerCallbacks* _pServerCallbacks;
  CharacteristicCallbacks* _pCharacteristicCallbacks;
  
  // Manipuladores Web
  void setupWebServer();
  String generateConfigPage();
  void handleConfigUpdate(AsyncWebServerRequest *request);
};

extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
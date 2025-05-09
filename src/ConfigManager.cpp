#include "ConfigManager.h"
#include <FS.h>
#include <SPIFFS.h>

// Define a macro para LittleFS
#define LittleFS SPIFFS

// Instância global
ConfigManager configManager;

// Construtor
ConfigManager::ConfigManager() {
  _isPortalActive = false;
  _isBLEActive = false;
  _bleConnected = false;
  _pServer = nullptr;
  _webServer = nullptr;
  _lastButtonCheckTime = 0;
  _lastButtonState = HIGH;
  
  // Inicializa callbacks
  _pServerCallbacks = new ServerCallbacks(this);
  _pCharacteristicCallbacks = new CharacteristicCallbacks(this);
}

// Inicialização
bool ConfigManager::begin() {
  // Inicializa sistema de arquivos
  if (!LittleFS.begin(true)) {
    Serial.println("Falha ao montar sistema de arquivos SPIFFS");
    Serial.println("Tentando formatar...");
    
    if (!SPIFFS.format()) {
      Serial.println("Formatação do SPIFFS falhou");
      return false;
    }
    
    if (!LittleFS.begin()) {
      Serial.println("Falha ao montar SPIFFS mesmo após formatação");
      return false;
    }
    
    Serial.println("SPIFFS formatado com sucesso");
  }
  
  // Carrega configuração ou usa padrão
  if (!loadConfig()) {
    Serial.println("Usando configuração padrão");
    resetToDefaults();
    saveConfig();
  }
  
  // Configura o pino do botão
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  
  return true;
}

// Carrega configuração do arquivo
bool ConfigManager::loadConfig() {
  String configJson = readFile("/config.json");
  if (configJson.isEmpty()) {
    return false;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configJson);
  
  if (error) {
    Serial.print("Falha ao deserializar JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Carrega dados do JSON para a estrutura - configurações básicas
  _config.deepSleepTimeMinutes = doc["sleep"] | DEFAULT_DEEP_SLEEP_TIME_MINUTES;
  _config.cpuFreqMHz = doc["cpu"] | DEFAULT_CPU_FREQ_MHZ;
  
  // Para valores float, precisamos tratar de forma diferente
  if (doc.containsKey("rain")) {
    _config.rainMmPerTip = doc["rain"].as<float>();
  } else {
    _config.rainMmPerTip = DEFAULT_RAIN_MM_PER_TIP;
  }
  
  // WiFi e nome do dispositivo
  strlcpy(_config.wifiSsid, doc["ssid"] | DEFAULT_WIFI_SSID, sizeof(_config.wifiSsid));
  strlcpy(_config.wifiPassword, doc["pass"] | DEFAULT_WIFI_PASSWORD, sizeof(_config.wifiPassword));
  strlcpy(_config.deviceName, doc["name"] | DEFAULT_DEVICE_NAME, sizeof(_config.deviceName));
  
  // Meshtastic
  strlcpy(_config.meshtasticNodeIP, doc["node_ip"] | DEFAULT_MESHTASTIC_NODE_IP, sizeof(_config.meshtasticNodeIP));
  _config.meshtasticNodePort = doc["node_port"] | DEFAULT_MESHTASTIC_NODE_PORT;
  
  // MQTT (novos campos)
  strlcpy(_config.mqttServer, doc["mqtt_server"] | DEFAULT_MQTT_SERVER, sizeof(_config.mqttServer));
  _config.mqttPort = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
  strlcpy(_config.mqttUsername, doc["mqtt_user"] | DEFAULT_MQTT_USERNAME, sizeof(_config.mqttUsername));
  strlcpy(_config.mqttPassword, doc["mqtt_pass"] | DEFAULT_MQTT_PASSWORD, sizeof(_config.mqttPassword));
  strlcpy(_config.mqttClientId, doc["mqtt_client"] | DEFAULT_MQTT_CLIENT_ID, sizeof(_config.mqttClientId));
  strlcpy(_config.mqttTopic, doc["mqtt_topic"] | DEFAULT_MQTT_TOPIC, sizeof(_config.mqttTopic));
  _config.mqttUpdateInterval = doc["mqtt_interval"] | DEFAULT_MQTT_UPDATE_INTERVAL;
  
  _config.configValid = true;
  return true;
}

// Salva configuração em arquivo
bool ConfigManager::saveConfig() {
  StaticJsonDocument<1024> doc;
  
  // Configurações básicas
  doc["sleep"] = _config.deepSleepTimeMinutes;
  doc["cpu"] = _config.cpuFreqMHz;
  doc["rain"] = _config.rainMmPerTip;
  doc["name"] = _config.deviceName;
  
  // WiFi
  doc["ssid"] = _config.wifiSsid;
  doc["pass"] = _config.wifiPassword;
  
  // Meshtastic
  doc["node_ip"] = _config.meshtasticNodeIP;
  doc["node_port"] = _config.meshtasticNodePort;
  
  // MQTT
  doc["mqtt_server"] = _config.mqttServer;
  doc["mqtt_port"] = _config.mqttPort;
  doc["mqtt_user"] = _config.mqttUsername;
  doc["mqtt_pass"] = _config.mqttPassword;
  doc["mqtt_client"] = _config.mqttClientId;
  doc["mqtt_topic"] = _config.mqttTopic;
  doc["mqtt_interval"] = _config.mqttUpdateInterval;
  
  String configJson;
  serializeJson(doc, configJson);
  
  return writeFile("/config.json", configJson.c_str());
}

// Reseta para valores padrão
void ConfigManager::resetToDefaults() {
  _config.deepSleepTimeMinutes = DEFAULT_DEEP_SLEEP_TIME_MINUTES;
  _config.cpuFreqMHz = DEFAULT_CPU_FREQ_MHZ;
  _config.rainMmPerTip = DEFAULT_RAIN_MM_PER_TIP;
  
  // WiFi e configurações básicas
  strlcpy(_config.wifiSsid, DEFAULT_WIFI_SSID, sizeof(_config.wifiSsid));
  strlcpy(_config.wifiPassword, DEFAULT_WIFI_PASSWORD, sizeof(_config.wifiPassword));
  strlcpy(_config.deviceName, DEFAULT_DEVICE_NAME, sizeof(_config.deviceName));
  
  // Meshtastic
  strlcpy(_config.meshtasticNodeIP, DEFAULT_MESHTASTIC_NODE_IP, sizeof(_config.meshtasticNodeIP));
  _config.meshtasticNodePort = DEFAULT_MESHTASTIC_NODE_PORT;
  
  // MQTT
  strlcpy(_config.mqttServer, DEFAULT_MQTT_SERVER, sizeof(_config.mqttServer));
  _config.mqttPort = DEFAULT_MQTT_PORT;
  strlcpy(_config.mqttUsername, DEFAULT_MQTT_USERNAME, sizeof(_config.mqttUsername));
  strlcpy(_config.mqttPassword, DEFAULT_MQTT_PASSWORD, sizeof(_config.mqttPassword));
  strlcpy(_config.mqttClientId, DEFAULT_MQTT_CLIENT_ID, sizeof(_config.mqttClientId));
  strlcpy(_config.mqttTopic, DEFAULT_MQTT_TOPIC, sizeof(_config.mqttTopic));
  _config.mqttUpdateInterval = DEFAULT_MQTT_UPDATE_INTERVAL;
  
  _config.configValid = true;
}

// Inicia servidor BLE
void ConfigManager::startBLEServer() {
  if (_isBLEActive) return;
  
  Serial.println("Iniciando servidor BLE...");
  
  // Inicializa BLE
  NimBLEDevice::init(_config.deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  
  // Cria servidor e serviço
  _pServer = NimBLEDevice::createServer();
  _pServer->setCallbacks(_pServerCallbacks);
  
  _pService = _pServer->createService(SERVICE_UUID);
  _pCharacteristic = _pService->createCharacteristic(
                        CONFIG_CHAR_UUID,
                        NIMBLE_PROPERTY::READ | 
                        NIMBLE_PROPERTY::WRITE
                      );
  
  _pCharacteristic->setCallbacks(_pCharacteristicCallbacks);
  
  // Inicializa com JSON de configuração atual
  StaticJsonDocument<1024> doc;
  // Configurações básicas
  doc["sleep"] = _config.deepSleepTimeMinutes;
  doc["cpu"] = _config.cpuFreqMHz;
  doc["rain"] = _config.rainMmPerTip;
  doc["name"] = _config.deviceName;
  
  // WiFi
  doc["ssid"] = _config.wifiSsid;
  doc["pass"] = "********"; // Não envie a senha real via BLE
  
  // Meshtastic
  doc["node_ip"] = _config.meshtasticNodeIP;
  doc["node_port"] = _config.meshtasticNodePort;
  
  // MQTT
  doc["mqtt_server"] = _config.mqttServer;
  doc["mqtt_port"] = _config.mqttPort;
  doc["mqtt_user"] = _config.mqttUsername;
  doc["mqtt_pass"] = "********"; // Não envie a senha real via BLE
  doc["mqtt_client"] = _config.mqttClientId;
  doc["mqtt_topic"] = _config.mqttTopic;
  doc["mqtt_interval"] = _config.mqttUpdateInterval;
  
  String configJson;
  serializeJson(doc, configJson);
  
  _pCharacteristic->setValue(configJson);
  
  // Inicia o serviço
  _pService->start();
  
  // Inicia anúncio
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  NimBLEDevice::startAdvertising();
  
  Serial.println("BLE inicializado. Aguardando conexões...");
  _isBLEActive = true;
}

// Para servidor BLE
void ConfigManager::stopBLEServer() {
  if (!_isBLEActive) return;
  
  NimBLEDevice::stopAdvertising();
  NimBLEDevice::deinit(true);
  _isBLEActive = false;
  _bleConnected = false;
  Serial.println("Servidor BLE parado");
}

// Verifica se BLE está conectado
bool ConfigManager::isBLEConnected() {
  return _bleConnected;
}

// Inicia portal de configuração
void ConfigManager::startConfigPortal() {
  if (_isPortalActive) return;
  
  Serial.println("Iniciando portal de configuração...");
  
  // Inicializa AP
  WiFi.mode(WIFI_AP);
  String apName = String("ESP32-Weather-") + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  WiFi.softAP(apName.c_str(), CONFIG_AP_PASSWORD);
  
  Serial.print("Portal iniciado em IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Configura servidor web
  _webServer = new AsyncWebServer(80);
  setupWebServer();
  _webServer->begin();
  
  _isPortalActive = true;
  _portalStartTime = millis();
}

// Para portal de configuração
void ConfigManager::stopConfigPortal() {
  if (!_isPortalActive) return;
  
  _webServer->end();
  delete _webServer;
  _webServer = nullptr;
  
  WiFi.softAPdisconnect(true);
  
  _isPortalActive = false;
  Serial.println("Portal de configuração parado");
}

// Verifica se portal está ativo
bool ConfigManager::isPortalActive() {
  return _isPortalActive;
}

// Gerencia portal de configuração
void ConfigManager::handlePortal() {
  if (!_isPortalActive) return;
  
  // Verifica timeout
  if (millis() - _portalStartTime > CONFIG_PORTAL_TIMEOUT * 1000) {
    Serial.println("Timeout do portal de configuração");
    stopConfigPortal();
  }
}

// Retorna ponteiro para configuração
WeatherStationConfig* ConfigManager::getConfig() {
  return &_config;
}

// Verifica se botão de configuração foi pressionado
bool ConfigManager::checkConfigButtonPressed() {
  // Apenas verifica a cada 100ms
  if (millis() - _lastButtonCheckTime < 100) {
    return false;
  }
  _lastButtonCheckTime = millis();
  
  bool currentState = digitalRead(CONFIG_BUTTON_PIN);
  
  // Detecta borda de descida
  if (_lastButtonState == HIGH && currentState == LOW) {
    _lastButtonState = currentState;
    return true;
  }
  
  _lastButtonState = currentState;
  return false;
}

// Lê arquivo do sistema de arquivos
String ConfigManager::readFile(const char* path) {
  Serial.printf("Lendo arquivo: %s\n", path);
  
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Falha ao abrir arquivo para leitura");
    return String();
  }
  
  String content = file.readString();
  file.close();
  
  return content;
}

// Escreve arquivo no sistema de arquivos
bool ConfigManager::writeFile(const char* path, const char* message) {
  Serial.printf("Escrevendo em arquivo: %s\n", path);
  
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Falha ao abrir arquivo para escrita");
    return false;
  }
  
  if (!file.print(message)) {
    Serial.println("Falha ao escrever");
    file.close();
    return false;
  }
  
  file.close();
  return true;
}

// Configura servidor web
void ConfigManager::setupWebServer() {
  // Root - Página de configuração
  _webServer->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateConfigPage());
  });
  
  // Handler para salvar configuração
  _webServer->on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleConfigUpdate(request);
  });
  
  // Suporte a arquivos estáticos (CSS, JS)
  _webServer->serveStatic("/", LittleFS, "/");
  
  // Manipulador 404
  _webServer->onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Página não encontrada");
  });
}

// Gera página HTML de configuração - versão otimizada para reduzir uso de memória
String ConfigManager::generateConfigPage() {
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>ESP32</title><style>"
                  "body{font-family:Arial;margin:0;padding:10px;background:#f7f7f7}"
                  "div{max-width:600px;margin:auto;background:#fff;padding:15px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,.1)}"
                  "h1{color:#06c;text-align:center}label{display:block;font-weight:700;margin-top:8px}"
                  "input,select{width:100%;padding:6px;margin:3px 0 10px;border:1px solid #ddd;border-radius:4px}"
                  "button{background:#06c;color:#fff;border:none;padding:8px;border-radius:4px;width:100%}"
                  ".s{border-bottom:1px solid #eee;padding-bottom:10px;margin-bottom:10px}"
                  "</style></head><body><div><h1>ESP32 Weather</h1><form action='/save' method='post'>");

  // Configurações Gerais
  html += F("<div class='s'><h3>Geral</h3>");
  html += F("<label>Nome:</label><input name='deviceName' value='");
  html += _config.deviceName;
  html += F("'><label>Sleep (min):</label><input type='number' name='deepSleep' min='1' max='360' value='");
  html += _config.deepSleepTimeMinutes;
  html += F("'><label>CPU (MHz):</label><select name='cpuFreq'>");
  html += _config.cpuFreqMHz == 80 ? F("<option value='80' selected>80</option>") : F("<option value='80'>80</option>");
  html += _config.cpuFreqMHz == 160 ? F("<option value='160' selected>160</option>") : F("<option value='160'>160</option>");
  html += F("</select><label>Rain (mm):</label><input type='number' name='rainMmPerTip' min='0.1' max='5' step='0.05' value='");
  html += String(_config.rainMmPerTip, 2);
  html += F("'></div>");
  
  // WiFi
  html += F("<div class='s'><h3>Wi-Fi</h3><label>SSID:</label><input name='wifiSsid' value='");
  html += _config.wifiSsid;
  html += F("'><label>Senha:</label><input type='password' name='wifiPassword' value='");
  html += _config.wifiPassword;
  html += F("'></div>");
  
  // Meshtastic
  html += F("<div class='s'><h3>Meshtastic</h3><label>IP:</label><input name='meshtasticNodeIP' value='");
  html += _config.meshtasticNodeIP;
  html += F("'><label>Porta:</label><input type='number' name='meshtasticNodePort' min='1' max='65535' value='");
  html += _config.meshtasticNodePort;
  html += F("'></div>");
  
  // MQTT
  html += F("<div class='s'><h3>MQTT</h3>");
  html += F("<label>Servidor:</label><input name='mqttServer' value='");
  html += _config.mqttServer;
  html += F("'><label>Porta:</label><input type='number' name='mqttPort' min='1' max='65535' value='");
  html += _config.mqttPort;
  html += F("'><label>Usuário:</label><input name='mqttUsername' value='");
  html += _config.mqttUsername;
  html += F("'><label>Senha:</label><input type='password' name='mqttPassword' value='");
  html += _config.mqttPassword;
  html += F("'><label>Client ID:</label><input name='mqttClientId' value='");
  html += _config.mqttClientId;
  html += F("'><label>Tópico:</label><input name='mqttTopic' value='");
  html += _config.mqttTopic;
  html += F("'><label>Intervalo (s):</label><input type='number' name='mqttUpdateInterval' min='0' max='3600' value='");
  html += _config.mqttUpdateInterval;
  html += F("'><p style='font-size:0.8em'>Defina 0 para enviar apenas uma vez antes do deep sleep</p>");
  html += F("</div>");
  
  // Botão Salvar
  html += F("<button type='submit'>Salvar</button></form></div></body></html>");
  
  return html;
}

// Processa atualização de configuração via web
void ConfigManager::handleConfigUpdate(AsyncWebServerRequest *request) {
  bool needsSave = false;
  
  // Lê parâmetros
  if (request->hasParam("deviceName", true)) {
    String deviceName = request->getParam("deviceName", true)->value();
    if (deviceName.length() > 0 && deviceName.length() < sizeof(_config.deviceName)) {
      strlcpy(_config.deviceName, deviceName.c_str(), sizeof(_config.deviceName));
      needsSave = true;
    }
  }
  
  if (request->hasParam("deepSleep", true)) {
    int deepSleep = request->getParam("deepSleep", true)->value().toInt();
    if (deepSleep >= 1 && deepSleep <= 360) {
      _config.deepSleepTimeMinutes = deepSleep;
      needsSave = true;
    }
  }
  
  if (request->hasParam("cpuFreq", true)) {
    int cpuFreq = request->getParam("cpuFreq", true)->value().toInt();
    if (cpuFreq == 80 || cpuFreq == 160) {
      _config.cpuFreqMHz = cpuFreq;
      needsSave = true;
    }
  }
  
  if (request->hasParam("rainMmPerTip", true)) {
    float rainMm = request->getParam("rainMmPerTip", true)->value().toFloat();
    if (rainMm >= 0.1 && rainMm <= 5.0) {
      _config.rainMmPerTip = rainMm;
      needsSave = true;
    }
  }
  
  if (request->hasParam("wifiSsid", true)) {
    String wifiSsid = request->getParam("wifiSsid", true)->value();
    if (wifiSsid.length() > 0 && wifiSsid.length() < sizeof(_config.wifiSsid)) {
      strlcpy(_config.wifiSsid, wifiSsid.c_str(), sizeof(_config.wifiSsid));
      needsSave = true;
    }
  }
  
  if (request->hasParam("wifiPassword", true)) {
    String wifiPassword = request->getParam("wifiPassword", true)->value();
    if (wifiPassword.length() > 0 && wifiPassword.length() < sizeof(_config.wifiPassword)) {
      strlcpy(_config.wifiPassword, wifiPassword.c_str(), sizeof(_config.wifiPassword));
      needsSave = true;
    }
  }
  
  if (request->hasParam("meshtasticNodeIP", true)) {
    String nodeIP = request->getParam("meshtasticNodeIP", true)->value();
    if (nodeIP.length() > 0 && nodeIP.length() < sizeof(_config.meshtasticNodeIP)) {
      strlcpy(_config.meshtasticNodeIP, nodeIP.c_str(), sizeof(_config.meshtasticNodeIP));
      needsSave = true;
    }
  }
  
  if (request->hasParam("meshtasticNodePort", true)) {
    int nodePort = request->getParam("meshtasticNodePort", true)->value().toInt();
    if (nodePort > 0 && nodePort < 65536) {
      _config.meshtasticNodePort = nodePort;
      needsSave = true;
    }
  }
  
  // Parâmetros MQTT
  if (request->hasParam("mqttServer", true)) {
    String mqttServer = request->getParam("mqttServer", true)->value();
    if (mqttServer.length() < sizeof(_config.mqttServer)) {
      strlcpy(_config.mqttServer, mqttServer.c_str(), sizeof(_config.mqttServer));
      needsSave = true;
    }
  }
  
  if (request->hasParam("mqttPort", true)) {
    int mqttPort = request->getParam("mqttPort", true)->value().toInt();
    if (mqttPort > 0 && mqttPort < 65536) {
      _config.mqttPort = mqttPort;
      needsSave = true;
    }
  }
  
  if (request->hasParam("mqttUsername", true)) {
    String mqttUsername = request->getParam("mqttUsername", true)->value();
    if (mqttUsername.length() < sizeof(_config.mqttUsername)) {
      strlcpy(_config.mqttUsername, mqttUsername.c_str(), sizeof(_config.mqttUsername));
      needsSave = true;
    }
  }
  
  if (request->hasParam("mqttPassword", true)) {
    String mqttPassword = request->getParam("mqttPassword", true)->value();
    if (mqttPassword.length() < sizeof(_config.mqttPassword)) {
      strlcpy(_config.mqttPassword, mqttPassword.c_str(), sizeof(_config.mqttPassword));
      needsSave = true;
    }
  }
  
  if (request->hasParam("mqttClientId", true)) {
    String mqttClientId = request->getParam("mqttClientId", true)->value();
    if (mqttClientId.length() > 0 && mqttClientId.length() < sizeof(_config.mqttClientId)) {
      strlcpy(_config.mqttClientId, mqttClientId.c_str(), sizeof(_config.mqttClientId));
      needsSave = true;
    }
  }
  
  if (request->hasParam("mqttTopic", true)) {
    String mqttTopic = request->getParam("mqttTopic", true)->value();
    if (mqttTopic.length() > 0 && mqttTopic.length() < sizeof(_config.mqttTopic)) {
      strlcpy(_config.mqttTopic, mqttTopic.c_str(), sizeof(_config.mqttTopic));
      needsSave = true;
    }
  }
  
  if (request->hasParam("mqttUpdateInterval", true)) {
    int mqttUpdateInterval = request->getParam("mqttUpdateInterval", true)->value().toInt();
    if (mqttUpdateInterval >= 0 && mqttUpdateInterval <= 3600) {
      _config.mqttUpdateInterval = mqttUpdateInterval;
      needsSave = true;
    }
  }
  
  if (needsSave) {
    saveConfig();
  }
  
  // Responde confirmando - versão minimalista
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Salvo</title><style>body{font-family:Arial;text-align:center;padding:20px;background:#f7f7f7}"
                  "div{max-width:320px;margin:auto;background:#fff;padding:15px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,.1)}"
                  "h2{color:#06c}p{margin:15px 0}.g{color:#4CAF50;font-weight:700}</style>"
                  "<meta http-equiv='refresh' content='3;url=/'></head><body><div>"
                  "<h2>ESP32 Weather</h2><p class='g'>Configurações salvas!</p>"
                  "<p>Reiniciando...</p></div></body></html>");
                  
  request->send(200, "text/html", html);
  
  // Agenda reinício do ESP32 para aplicar as novas configurações
  Serial.println("Configurações atualizadas. Reiniciando em 5 segundos...");
  delay(1000); // Dá tempo para enviar a resposta HTTP
  ESP.restart();
}

// Callbacks para servidor BLE
void ConfigManager::ServerCallbacks::onConnect(NimBLEServer* pServer) {
  _configManager->_bleConnected = true;
  Serial.println("Cliente BLE conectado");
}

void ConfigManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
  _configManager->_bleConnected = false;
  Serial.println("Cliente BLE desconectado");
  
  // Reinicia anúncio para nova conexão
  NimBLEDevice::startAdvertising();
}

// Callbacks para características BLE
void ConfigManager::CharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
  std::string value = pCharacteristic->getValue();
  
  if (value.length() > 0) {
    Serial.println("Recebido via BLE:");
    Serial.println(value.c_str());
    
    // Analisa JSON recebido
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, value.c_str());
    
    if (error) {
      Serial.print("Falha ao analisar JSON: ");
      Serial.println(error.c_str());
      return;
    }
    
    WeatherStationConfig* config = _configManager->getConfig();
    bool needsSave = false;
    
    // Atualiza a configuração com novos valores
    if (doc.containsKey("sleep")) {
      uint8_t sleep = doc["sleep"];
      if (sleep >= 1 && sleep <= 360) {
        config->deepSleepTimeMinutes = sleep;
        needsSave = true;
      }
    }
    
    if (doc.containsKey("cpu")) {
      uint16_t cpu = doc["cpu"];
      if (cpu == 80 || cpu == 160) {
        config->cpuFreqMHz = cpu;
        needsSave = true;
      }
    }
    
    if (doc.containsKey("rain")) {
      float rain = doc["rain"];
      if (rain >= 0.1 && rain <= 5.0) {
        config->rainMmPerTip = rain;
        needsSave = true;
      }
    }
    
    if (doc.containsKey("ssid")) {
      const char* ssid = doc["ssid"];
      if (strlen(ssid) > 0 && strlen(ssid) < sizeof(config->wifiSsid)) {
        strlcpy(config->wifiSsid, ssid, sizeof(config->wifiSsid));
        needsSave = true;
      }
    }
    
    if (doc.containsKey("pass")) {
      const char* pass = doc["pass"];
      if (strcmp(pass, "********") != 0 && strlen(pass) > 0 && strlen(pass) < sizeof(config->wifiPassword)) {
        strlcpy(config->wifiPassword, pass, sizeof(config->wifiPassword));
        needsSave = true;
      }
    }
    
    if (doc.containsKey("node_ip")) {
      const char* nodeIP = doc["node_ip"];
      if (strlen(nodeIP) > 0 && strlen(nodeIP) < sizeof(config->meshtasticNodeIP)) {
        strlcpy(config->meshtasticNodeIP, nodeIP, sizeof(config->meshtasticNodeIP));
        needsSave = true;
      }
    }
    
    if (doc.containsKey("node_port")) {
      uint16_t nodePort = doc["node_port"];
      if (nodePort > 0 && nodePort < 65536) {
        config->meshtasticNodePort = nodePort;
        needsSave = true;
      }
    }
    
    if (doc.containsKey("name")) {
      const char* name = doc["name"];
      if (strlen(name) > 0 && strlen(name) < sizeof(config->deviceName)) {
        strlcpy(config->deviceName, name, sizeof(config->deviceName));
        needsSave = true;
      }
    }
    
    if (needsSave) {
      _configManager->saveConfig();
      
      // Atualiza característica com os novos valores
      StaticJsonDocument<512> respDoc;
      respDoc["sleep"] = config->deepSleepTimeMinutes;
      respDoc["cpu"] = config->cpuFreqMHz;
      respDoc["rain"] = config->rainMmPerTip;
      respDoc["ssid"] = config->wifiSsid;
      respDoc["pass"] = "********";
      respDoc["node_ip"] = config->meshtasticNodeIP;
      respDoc["node_port"] = config->meshtasticNodePort;
      respDoc["name"] = config->deviceName;
      respDoc["status"] = "updated";
      
      String respJson;
      serializeJson(respDoc, respJson);
      
      pCharacteristic->setValue(respJson.c_str());
      
      Serial.println("Configuração atualizada via BLE");
    } else {
      // Envia resposta de erro
      StaticJsonDocument<128> respDoc;
      respDoc["status"] = "error";
      respDoc["msg"] = "Invalid parameters";
      
      String respJson;
      serializeJson(respDoc, respJson);
      
      pCharacteristic->setValue(respJson.c_str());
    }
  }
}

void ConfigManager::CharacteristicCallbacks::onRead(NimBLECharacteristic* pCharacteristic) {
  Serial.println("Leitura BLE solicitada");
  
  WeatherStationConfig* config = _configManager->getConfig();
  
  // Atualiza característica com os valores atuais
  StaticJsonDocument<1024> doc;
  // Configurações básicas
  doc["sleep"] = config->deepSleepTimeMinutes;
  doc["cpu"] = config->cpuFreqMHz;
  doc["rain"] = config->rainMmPerTip;
  doc["name"] = config->deviceName;
  
  // WiFi
  doc["ssid"] = config->wifiSsid;
  doc["pass"] = "********"; // Não envia a senha real via BLE
  
  // Meshtastic
  doc["node_ip"] = config->meshtasticNodeIP;
  doc["node_port"] = config->meshtasticNodePort;
  
  // MQTT
  doc["mqtt_server"] = config->mqttServer;
  doc["mqtt_port"] = config->mqttPort;
  doc["mqtt_user"] = config->mqttUsername;
  doc["mqtt_pass"] = "********"; // Não envia a senha real via BLE
  doc["mqtt_client"] = config->mqttClientId;
  doc["mqtt_topic"] = config->mqttTopic;
  doc["mqtt_interval"] = config->mqttUpdateInterval;
  
  String configJson;
  serializeJson(doc, configJson);
  
  pCharacteristic->setValue(configJson.c_str());
}
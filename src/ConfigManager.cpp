#include "ConfigManager.h"
#include <FS.h>
#include <LITTLEFS.h>

// Define a macro para LittleFS
#define LittleFS LITTLEFS

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
    Serial.println("Falha ao montar sistema de arquivos LittleFS");
    return false;
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
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configJson);
  
  if (error) {
    Serial.print("Falha ao deserializar JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Carrega dados do JSON para a estrutura
  _config.deepSleepTimeMinutes = doc["sleep"] | DEFAULT_DEEP_SLEEP_TIME_MINUTES;
  _config.cpuFreqMHz = doc["cpu"] | DEFAULT_CPU_FREQ_MHZ;
  
  // Para valores float, precisamos tratar de forma diferente
  if (doc.containsKey("rain")) {
    _config.rainMmPerTip = doc["rain"].as<float>();
  } else {
    _config.rainMmPerTip = DEFAULT_RAIN_MM_PER_TIP;
  }
  
  strlcpy(_config.wifiSsid, doc["ssid"] | DEFAULT_WIFI_SSID, sizeof(_config.wifiSsid));
  strlcpy(_config.wifiPassword, doc["pass"] | DEFAULT_WIFI_PASSWORD, sizeof(_config.wifiPassword));
  strlcpy(_config.meshtasticNodeIP, doc["node_ip"] | DEFAULT_MESHTASTIC_NODE_IP, sizeof(_config.meshtasticNodeIP));
  _config.meshtasticNodePort = doc["node_port"] | DEFAULT_MESHTASTIC_NODE_PORT;
  strlcpy(_config.deviceName, doc["name"] | DEFAULT_DEVICE_NAME, sizeof(_config.deviceName));
  
  _config.configValid = true;
  return true;
}

// Salva configuração em arquivo
bool ConfigManager::saveConfig() {
  StaticJsonDocument<512> doc;
  
  doc["sleep"] = _config.deepSleepTimeMinutes;
  doc["cpu"] = _config.cpuFreqMHz;
  doc["rain"] = _config.rainMmPerTip;
  doc["ssid"] = _config.wifiSsid;
  doc["pass"] = _config.wifiPassword;
  doc["node_ip"] = _config.meshtasticNodeIP;
  doc["node_port"] = _config.meshtasticNodePort;
  doc["name"] = _config.deviceName;
  
  String configJson;
  serializeJson(doc, configJson);
  
  return writeFile("/config.json", configJson.c_str());
}

// Reseta para valores padrão
void ConfigManager::resetToDefaults() {
  _config.deepSleepTimeMinutes = DEFAULT_DEEP_SLEEP_TIME_MINUTES;
  _config.cpuFreqMHz = DEFAULT_CPU_FREQ_MHZ;
  _config.rainMmPerTip = DEFAULT_RAIN_MM_PER_TIP;
  
  strlcpy(_config.wifiSsid, DEFAULT_WIFI_SSID, sizeof(_config.wifiSsid));
  strlcpy(_config.wifiPassword, DEFAULT_WIFI_PASSWORD, sizeof(_config.wifiPassword));
  strlcpy(_config.meshtasticNodeIP, DEFAULT_MESHTASTIC_NODE_IP, sizeof(_config.meshtasticNodeIP));
  _config.meshtasticNodePort = DEFAULT_MESHTASTIC_NODE_PORT;
  strlcpy(_config.deviceName, DEFAULT_DEVICE_NAME, sizeof(_config.deviceName));
  
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
  StaticJsonDocument<512> doc;
  doc["sleep"] = _config.deepSleepTimeMinutes;
  doc["cpu"] = _config.cpuFreqMHz;
  doc["rain"] = _config.rainMmPerTip;
  doc["ssid"] = _config.wifiSsid;
  doc["pass"] = "********"; // Não envie a senha real via BLE
  doc["node_ip"] = _config.meshtasticNodeIP;
  doc["node_port"] = _config.meshtasticNodePort;
  doc["name"] = _config.deviceName;
  
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

// Gera página HTML de configuração
String ConfigManager::generateConfigPage() {
  String html = F("<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>Configuração ESP32 Weather Station</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f7f7f7; }"
                  ".container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1); }"
                  "h1 { color: #0066cc; margin-top: 0; text-align: center; }"
                  "label { display: block; margin-top: 10px; font-weight: bold; }"
                  "input, select { width: 100%; padding: 8px; margin-top: 5px; margin-bottom: 15px; box-sizing: border-box; border: 1px solid #ddd; border-radius: 4px; }"
                  "button { background-color: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; width: 100%; }"
                  "button:hover { background-color: #0052a3; }"
                  ".section { border-bottom: 1px solid #eee; padding-bottom: 15px; margin-bottom: 15px; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>ESP32 Weather Station</h1>"
                  "<form action='/save' method='post'>"
                  
                  "<div class='section'>"
                  "<h2>Configurações Gerais</h2>"
                  "<label for='deviceName'>Nome do Dispositivo:</label>"
                  "<input type='text' id='deviceName' name='deviceName' value='");
  html += _config.deviceName;
  html += F("'>"
            
            "<label for='deepSleep'>Tempo entre Leituras (minutos):</label>"
            "<input type='number' id='deepSleep' name='deepSleep' min='1' max='360' value='");
  html += _config.deepSleepTimeMinutes;
  html += F("'>"
            
            "<label for='cpuFreq'>Frequência da CPU (MHz):</label>"
            "<select id='cpuFreq' name='cpuFreq'>");
  html += _config.cpuFreqMHz == 80 ? F("<option value='80' selected>80 MHz</option>") : F("<option value='80'>80 MHz</option>");
  html += _config.cpuFreqMHz == 160 ? F("<option value='160' selected>160 MHz</option>") : F("<option value='160'>160 MHz</option>");
  html += F("</select>"
            
            "<label for='rainMmPerTip'>Calibração Pluviômetro (mm por basculada):</label>"
            "<input type='number' id='rainMmPerTip' name='rainMmPerTip' min='0.1' max='5' step='0.05' value='");
  html += String(_config.rainMmPerTip, 2);
  html += F("'>"
            "</div>"
            
            "<div class='section'>"
            "<h2>Configurações Wi-Fi</h2>"
            "<label for='wifiSsid'>SSID:</label>"
            "<input type='text' id='wifiSsid' name='wifiSsid' value='");
  html += _config.wifiSsid;
  html += F("'>"
            
            "<label for='wifiPassword'>Senha:</label>"
            "<input type='password' id='wifiPassword' name='wifiPassword' value='");
  html += _config.wifiPassword;
  html += F("'>"
            "</div>"
            
            "<div class='section'>"
            "<h2>Configurações Meshtastic</h2>"
            "<label for='meshtasticNodeIP'>IP do Nó Meshtastic:</label>"
            "<input type='text' id='meshtasticNodeIP' name='meshtasticNodeIP' value='");
  html += _config.meshtasticNodeIP;
  html += F("'>"
            
            "<label for='meshtasticNodePort'>Porta:</label>"
            "<input type='number' id='meshtasticNodePort' name='meshtasticNodePort' min='1' max='65535' value='");
  html += _config.meshtasticNodePort;
  html += F("'>"
            "</div>"
            
            "<button type='submit'>Salvar Configurações</button>"
            "</form>"
            "</div>"
            "</body>"
            "</html>");
            
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
  
  if (needsSave) {
    saveConfig();
  }
  
  // Responde confirmando
  String html = F("<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>Configuração Salva</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f7f7f7; text-align: center; }"
                  ".container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1); }"
                  "h1 { color: #0066cc; }"
                  "p { margin-bottom: 20px; }"
                  ".message { color: #4CAF50; font-weight: bold; }"
                  "a { display: inline-block; background-color: #0066cc; color: white; padding: 10px 15px; text-decoration: none; border-radius: 4px; margin-top: 20px; }"
                  "a:hover { background-color: #0052a3; }"
                  "</style>"
                  "<meta http-equiv='refresh' content='3;url=/'>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>ESP32 Weather Station</h1>"
                  "<p class='message'>Configurações salvas com sucesso!</p>"
                  "<p>O dispositivo reiniciará em alguns segundos...</p>"
                  "<a href='/'>Voltar</a>"
                  "</div>"
                  "</body>"
                  "</html>");
                  
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
  StaticJsonDocument<512> doc;
  doc["sleep"] = config->deepSleepTimeMinutes;
  doc["cpu"] = config->cpuFreqMHz;
  doc["rain"] = config->rainMmPerTip;
  doc["ssid"] = config->wifiSsid;
  doc["pass"] = "********"; // Não envia a senha real
  doc["node_ip"] = config->meshtasticNodeIP;
  doc["node_port"] = config->meshtasticNodePort;
  doc["name"] = config->deviceName;
  
  String configJson;
  serializeJson(doc, configJson);
  
  pCharacteristic->setValue(configJson.c_str());
}
/*
 * ESP32 Weather Station with Meshtastic Integration
 * 
 * This code implements a weather station that:
 * - Runs for a specified time and then enters deep sleep for 5 minutes (configurable)
 * - Sets CPU frequency to 160MHz for power efficiency
 * - Reads temperature and humidity from a DHT22 sensor
 * - Counts rain with a tipping bucket rain gauge (0.25mm per tip)
 * - Wakes up when rain gauge triggers an interrupt
 * - Transmits data in JSON format to a Meshtastic node via WiFi for LoRa propagation
 * - Returns to deep sleep mode after execution to conserve power
 * 
 * The ESP32 wakes up:
 * - Every 5 minutes (configurable)
 * - When the rain gauge triggers an interrupt
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <time.h>
#include "config.h"
#include "ConfigManager.h"

// Inclui suporte a Meshtastic se a flag estiver definida
#ifdef USE_MESHTASTIC
  #include "meshtastic-protobuf.h"
#endif

// Inclui suporte a MQTT se a flag estiver definida
#ifdef USE_MQTT
  #include <PubSubClient.h>
  WiFiClient wifiClient;
  PubSubClient mqttClient(wifiClient);
#endif

// Sensor-specific includes and initialization
#ifdef USE_DHT22
    #include <DHT.h>
    DHT dht(DHT_PIN, DHT22);
#endif

#ifdef USE_AHT20
    #include <Adafruit_AHTX0.h>
    Adafruit_AHTX0 aht;
#endif

#ifdef USE_BMP280
    #include <Adafruit_BMP280.h>
    Adafruit_BMP280 bmp;
#endif

// Estrutura para armazenar um registro de chuva
struct RainRecord {
  uint32_t timestamp;  // Timestamp em milissegundos desde o início (ou em segundos desde o epoch)
  float amount;        // Quantidade de chuva em mm
};

// Define RTC variables that persist through deep sleep
RTC_DATA_ATTR int rainCounter = 0;    // Rain counter
RTC_DATA_ATTR bool isFirstRun = true; // Flag for first run after power-on
RTC_DATA_ATTR bool needsConfiguration = false; // Flag to enter configuration mode
RTC_DATA_ATTR uint32_t lastResetTime = 0;  // Último tempo em que o sistema foi resetado (em segundos)
RTC_DATA_ATTR time_t lastNTPSync = 0;      // Última vez que sincronizamos com NTP (timestamp Unix)
RTC_DATA_ATTR RainRecord rainHistory[MAX_RAIN_RECORDS]; // Histórico de registros de chuva
RTC_DATA_ATTR int rainHistoryCount = 0;    // Número atual de registros no histórico
RTC_DATA_ATTR float totalRainfall = 0.0;   // Total acumulado de chuva (mm)

// Define wake-up sources
#define TIMER_WAKEUP 1
#define EXTERNAL_WAKEUP 2
#define BUTTON_WAKEUP 3
RTC_DATA_ATTR int wakeupReason = 0;

// Variables for runtime management
unsigned long startTime; // To track how long the device has been running
float rainLastHour = 0.0;
float rainLast24Hours = 0.0;

// Define battery monitoring variables
#define BATTERY_ADC_PIN 35  // GPIO35 (ADC1_CH7)
#define ADC_ATTEN ADC_ATTEN_DB_12  // Atenuação de 11dB para leitura até ~3.6V
#define ADC_WIDTH ADC_WIDTH_BIT_12  // Resolução de 12 bits (0-4095)

// Function prototypes
void setupWiFi();
void setupSensors();
bool readSensorData(float &temperature, float &humidity);
void addRainRecord(float amount);
float getRainLastHour();
float getRainLast24Hours();
void manageRainHistory();

#ifdef USE_DHT22
bool readDHT22(float &temperature, float &humidity);
#endif

#ifdef USE_AHT20
bool readAHT20(float &temperature, float &humidity);
#endif

#ifdef USE_BMP280
bool readBMP280(float &temperature, float &humidity);
#endif

#ifdef USE_MESHTASTIC
void sendDataToMeshtastic(float temperature, float humidity, float rainAmount);
#endif

#ifdef USE_MQTT
bool sendDataToMQTT(float temperature, float humidity, float rainAmount);
#endif
void printWakeupReason();
void setupDeepSleep();
void setCpuFrequency();
bool shouldEnterSleep();
float getBatteryVoltage();
int batteryLevel(float voltage);
void syncTimeWithNTP();
time_t getLocalTime();

void setup() {
  // Record the start time
  startTime = millis();
  
  // Initialize serial communication
  Serial.begin(115200);
  
  // Short delay to allow serial to initialize
  delay(1000);
  
  Serial.println("\n\nESP32 Weather Station Starting...");
  
  // Set ADC resolution to battery monitoring
  analogReadResolution(12);  // Define resolução de 12 bits
  analogSetAttenuation(ADC_11db);  // Define atenuação

  // Initialize ConfigManager
  if (!configManager.begin()) {
    Serial.println("Failed to initialize ConfigManager!");
    // Continue with default values
  }
  
  // Determine wake-up reason
  printWakeupReason();
  
  // Get current configuration
  WeatherStationConfig* config = configManager.getConfig();
  
  // Connect to WiFi and sync time
  setupWiFi();

  // Check if device should enter config mode
  if (needsConfiguration || configManager.checkConfigButtonPressed()) {
    Serial.println("Entering configuration mode...");
    
    #ifdef USE_CONFIG_PORTAL
      // Start configuration interfaces
      configManager.startBLEServer();
      configManager.startConfigPortal();
      
      unsigned long configStartTime = millis();
      
      // Stay in config mode for CONFIG_PORTAL_TIMEOUT seconds or until button pressed again
      while (millis() - configStartTime < (CONFIG_PORTAL_TIMEOUT * 1000) && 
             !configManager.checkConfigButtonPressed()) {
        // Handle config portal
        configManager.handlePortal();
        delay(100);
      }
      
      // Clean up
      configManager.stopConfigPortal();
      configManager.stopBLEServer();
      
      Serial.println("Exiting configuration mode");
      
      // Return to normal operation
      ESP.restart();
      return;
    #else
      Serial.println("Configuration portal not enabled in this build");
    #endif
  }
  
  // Set CPU frequency to value from configuration
  setCpuFrequency();
  
  // If this is the first run after power-on, initialize rain counter
  if (isFirstRun) {
    Serial.println("First run after power-on, initializing rain counter");
    rainCounter = 0;
    isFirstRun = false;
  }
  
  // Initialize sensors
  setupSensors();
  
  // Read sensor data
  float temperature = 0.0;
  float humidity = 0.0;
  float rainAmount = 0.0;
  
  // Check if we should enter sleep
  if (shouldEnterSleep()) {
    setupDeepSleep();
    Serial.println("Maximum runtime exceeded, entering deep sleep...");
    esp_deep_sleep_start();
    return; // This will never be reached
  }
  
  // Read temperature and humidity
  bool sensorReadSuccess = readSensorData(temperature, humidity);
  
  // Check again if we should enter sleep after sensor reading
  if (shouldEnterSleep()) {
    setupDeepSleep();
    Serial.println("Maximum runtime exceeded after sensor reading, entering deep sleep...");
    esp_deep_sleep_start();
    return; // This will never be reached
  }
  
  // Calculate rain amount
  float newRainAmount = 0.0;
  if (wakeupReason == EXTERNAL_WAKEUP) {
    // Se acordou por interrupção, incrementa o contador de chuva
    rainCounter++;
    Serial.println("Rain detected! Counter: " + String(rainCounter));
    
    // Calcula a quantidade equivalente deste registro e adiciona ao histórico
    newRainAmount = config->rainMmPerTip;
    addRainRecord(newRainAmount);
  }
  
  // Gerencia o histórico de registros de chuva (elimina registros muito antigos)
  // Isto é feito em todas as execuções, não apenas quando chove
  manageRainHistory();
  
  // Calcula quantidade total de chuva com calibração da configuração
  rainAmount = rainCounter * config->rainMmPerTip;
  
  // Calcula chuva na última hora e nas últimas 24 horas
  // Estas funções agora retornam valores atualizados independentemente da causa do wakeup
  /* float rainLastHour = getRainLastHour();
  float rainLast24Hours = getRainLast24Hours();
  
  Serial.print("Chuva total acumulada: ");
  Serial.print(rainAmount);
  Serial.println(" mm");
  Serial.print("Chuva na última hora: ");
  Serial.print(rainLastHour);
  Serial.println(" mm");
  Serial.print("Chuva nas últimas 24 horas: ");
  Serial.print(rainLast24Hours);
  Serial.println(" mm"); */
  
  // Check if we should enter sleep
  if (shouldEnterSleep()) {
    setupDeepSleep();
    Serial.println("Maximum runtime exceeded after WiFi connection, entering deep sleep...");
    esp_deep_sleep_start();
    return; // This will never be reached
  }
  
  // Atualiza os cálculos de chuva após ter o timestamp NTP sincronizado
  if (WiFi.status() == WL_CONNECTED) {
    // Recalcula os valores de chuva com o timestamp atualizado
    rainLastHour = getRainLastHour();
    rainLast24Hours = getRainLast24Hours();    
    /* Serial.println("Valores de chuva recalculados após sincronização NTP:");
    Serial.print("Chuva na última hora: ");
    Serial.print(rainLastHour);
    Serial.println(" mm");
    Serial.print("Chuva nas últimas 24 horas: ");
    Serial.print(rainLast24Hours);
    Serial.println(" mm"); */
  }
  
  // Send data via MQTT or Meshtastic based on build configuration
  if (WiFi.status() == WL_CONNECTED) {
    #ifdef USE_MQTT
      // Use MQTT if enabled in build
      if (sendDataToMQTT(temperature, humidity, rainAmount)) {
        Serial.println("Data successfully sent via MQTT");
      } else {
        // Fall back to Meshtastic if MQTT fails and Meshtastic is available
        Serial.println("MQTT failed");
        #ifdef USE_MESHTASTIC
          Serial.println("Falling back to Meshtastic");
          sendDataToMeshtastic(temperature, humidity, rainAmount);
        #else
          Serial.println("No fallback available");
        #endif
      }
    #else
      // Use Meshtastic by default
      sendDataToMeshtastic(temperature, humidity, rainAmount);
    #endif
    
    // Disconnect WiFi before sleep to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  
  // Configure deep sleep
  setupDeepSleep();
  
  // Enter deep sleep
  Serial.println("Task completed, entering deep sleep...");
  esp_deep_sleep_start();
}

void loop() {
  // This will never run as the device enters deep sleep at the end of setup()
}

// Set CPU frequency to value from configuration
void setCpuFrequency() {
  WeatherStationConfig* config = configManager.getConfig();
  uint16_t cpuFreq = config->cpuFreqMHz;
  
  Serial.print("Setting CPU frequency to ");
  Serial.print(cpuFreq);
  Serial.print("MHz...");
  if (setCpuFrequencyMhz(cpuFreq)) {
    Serial.println("Success!");
  } else {
    Serial.println("Failed!");
  }
  Serial.print("Current CPU frequency: ");
  Serial.println(getCpuFrequencyMhz());
}

// Check if the maximum runtime has been exceeded
bool shouldEnterSleep() {
  unsigned long currentTime = millis();
  unsigned long runTime = currentTime - startTime;
  
  if (runTime >= MAX_RUNTIME_MS) {
    Serial.print("Runtime: ");
    Serial.print(runTime);
    Serial.print("ms exceeds maximum allowed runtime of ");
    Serial.print(MAX_RUNTIME_MS);
    Serial.println("ms");
    return true;
  }
  
  return false;
}

// Print wake-up reason and set global variable
void printWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO (rain gauge)");
      wakeupReason = EXTERNAL_WAKEUP;
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL (config button)");
      wakeupReason = BUTTON_WAKEUP;
      needsConfiguration = true;
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      wakeupReason = TIMER_WAKEUP;
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      wakeupReason = 0;
      break;
  }
}

// Connect to WiFi
void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  
  // Get WiFi credentials from config
  WeatherStationConfig* config = configManager.getConfig();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config->wifiSsid, config->wifiPassword);
  
  Serial.print("Connecting to: ");
  Serial.println(config->wifiSsid);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttemptTime < WIFI_TIMEOUT) {
    Serial.print(".");
    delay(100);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    
    // Sincronizar o relógio com NTP após conectar WiFi
    syncTimeWithNTP();
  } else {
    Serial.println();
    Serial.println("Connection failed! Starting configuration portal...");
    
    #ifdef USE_CONFIG_PORTAL
      // Disconnect failed WiFi connection
      WiFi.disconnect(true);
      
      // Start configuration interfaces
      configManager.startBLEServer();
      configManager.startConfigPortal();
      
      unsigned long configStartTime = millis();
      
      // Stay in config mode for CONFIG_PORTAL_TIMEOUT seconds or until button pressed again
      while (millis() - configStartTime < (CONFIG_PORTAL_TIMEOUT * 1000) && 
             !configManager.checkConfigButtonPressed()) {
        // Handle config portal
        configManager.handlePortal();
        delay(100);
      }
      
      // Clean up
      configManager.stopConfigPortal();
      configManager.stopBLEServer();
      
      Serial.println("Exiting configuration mode after WiFi failure");
      
      // Restart device to try with new settings
      ESP.restart();
    #else
      Serial.println("Configuration portal not enabled in this build");
    #endif
  }
}

#ifdef USE_DHT22
// Read temperature and humidity from DHT22 sensor
bool readDHT22(float &temperature, float &humidity) {
  Serial.println("Reading DHT22 sensor...");
  
  // Read values multiple times for reliability
  for (int i = 0; i < 3; i++) {
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    
    if (!isnan(humidity) && !isnan(temperature)) {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");
      return true;
    }
    
    Serial.println("Failed to read from DHT sensor, retrying...");
    delay(2000);
  }
  
  Serial.println("All attempts to read DHT sensor failed!");
  return false;
}
#endif

#ifdef USE_MESHTASTIC
// Send data to Meshtastic node using the toRadio API endpoint with proper protobuf structure
void sendDataToMeshtastic(float temperature, float humidity, float rainAmount) {
  Serial.println("Preparing data for Meshtastic node...");
  
  // Get Meshtastic configuration
  WeatherStationConfig* config = configManager.getConfig();
  
  // Create JSON document for the weather data
  StaticJsonDocument<256> dataDoc;
  
  // Include temperature data
  dataDoc["temperature"] = temperature;
  
  // Include sensor-specific data
  #ifdef USE_DHT22
    dataDoc["humidity"] = humidity;
    dataDoc["sensor"] = "DHT22";
  #endif
  
  // When both I2C sensors are used, we get more complete data
  #if defined(USE_AHT20) && defined(USE_BMP280)
    dataDoc["humidity"] = humidity;
    float pressure = bmp.readPressure() / 100.0F; // Convert Pa to hPa
    dataDoc["pressure"] = pressure;
    dataDoc["sensor"] = "AHT20+BMP280";
  #elif defined(USE_AHT20)
    dataDoc["humidity"] = humidity;
    dataDoc["sensor"] = "AHT20";
  #elif defined(USE_BMP280)
    dataDoc["pressure"] = bmp.readPressure() / 100.0F; // Convert Pa to hPa
    dataDoc["sensor"] = "BMP280";
  #endif
  
  // Include rain data and node identification
  dataDoc["rain"] = rainAmount;
  dataDoc["rain_1h"] = getRainLastHour();
  dataDoc["rain_24h"] = getRainLast24Hours();
  dataDoc["node_name"] = config->deviceName;
  
  // Adiciona apenas timestamp NTP (Unix) se disponível
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    time_t currentTime = getLocalTime();
    dataDoc["timestamp"] = currentTime;  // Timestamp Unix (segundos desde 1970)
  }
  
  // Add battery data
  dataDoc["voltage"] = getBatteryVoltage();
  dataDoc["BatteryLevel"] = batteryLevel(getBatteryVoltage());
  
  // Serialize weather data JSON to string
  String dataString;
  serializeJson(dataDoc, dataString);
  
  Serial.print("Weather data: ");
  Serial.println(dataString);
  
  // ===== Usar estrutura protobuf para Meshtastic =====
  
  // Criamos um pacote meteorológico usando o helper de nossa biblioteca
  // 0 = ID de nó automático (usa o ID configurado no próprio dispositivo)
  MeshPacket meshPacket = createWeatherDataPacket(dataString, 0);
  
  // Adicionar log para debug
  Serial.print("Criado pacote de dados meteorológicos com ID: ");
  Serial.println(meshPacket.id);
  Serial.print("Tamanho dos dados: ");
  Serial.print(meshPacket.payload.size);
  Serial.print(" bytes de ");
  Serial.print(sizeof(meshPacket.payload.data));
  Serial.println(" disponíveis");
  
  // Converter a estrutura MeshPacket em JSON para a API HTTP do Meshtastic
  String toRadioJson = createMeshtasticToRadioJson(meshPacket);
  
  // Send data to Meshtastic node
  Serial.println("Sending data to Meshtastic node...");
  
  HTTPClient http;
  String url = "http://" + String(config->meshtasticNodeIP) + ":" + 
               String(config->meshtasticNodePort) + MESHTASTIC_API_ENDPOINT;
  
  Serial.print("Sending to URL: ");
  Serial.println(url);
  Serial.print("ToRadio payload (protobuf): ");
  Serial.println(toRadioJson);
  
  // Verificar se consegue conectar ao host Meshtastic
  bool hostReachable = false;
  
  Serial.print("Verificando conexão com o host Meshtastic: ");
  Serial.println(config->meshtasticNodeIP);
  
  IPAddress host;
  if (host.fromString(config->meshtasticNodeIP)) {
    // Tentar conexão TCP direta para verificar acessibilidade
    WiFiClient client;
    if (client.connect(config->meshtasticNodeIP, config->meshtasticNodePort)) {
      Serial.println("Conexão TCP bem-sucedida, host está acessível!");
      client.stop();
      hostReachable = true;
    } else {
      Serial.println("Falha na conexão TCP. Host inacessível.");
    }
  } else {
    Serial.println("Endereço IP inválido!");
  }
  
  // Só tenta enviar se o host estiver acessível
  int httpResponseCode = -1;
  
  if (hostReachable) {
    // Configurar timeout de conexão maior
    http.setConnectTimeout(10000); // 10 segundos
    http.setTimeout(10000);        // 10 segundos para operações
    
    // Iniciar a conexão e enviar a requisição
    if (http.begin(url)) {
      http.addHeader("Content-Type", "application/json");
    
      // The toRadio endpoint uses PUT request
      httpResponseCode = http.PUT(toRadioJson);
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println("Response: " + payload);
        
        if (httpResponseCode == 200 || httpResponseCode == 204) {
          Serial.println("Message sent successfully to Meshtastic node!");
        } else {
          Serial.println("Unexpected response from Meshtastic node.");
        }
      } else {
        Serial.print("Error on sending PUT: ");
        Serial.println(httpResponseCode);
        // Exibir mais detalhes sobre o erro
        switch (httpResponseCode) {
          case -1: Serial.println("CONNECTION REFUSED"); break;
          case -2: Serial.println("SEND HEADER FAILED"); break;
          case -3: Serial.println("SEND PAYLOAD FAILED"); break;
          case -4: Serial.println("NOT CONNECTED"); break;
          case -5: Serial.println("CONNECTION LOST"); break;
          case -6: Serial.println("NO STREAM"); break;
          case -7: Serial.println("NO HTTP SERVER"); break;
          case -8: Serial.println("TOO LESS RAM"); break;
          case -9: Serial.println("ENCODING ERROR"); break;
          case -10: Serial.println("STREAM WRITE ERROR"); break;
          case -11: Serial.println("CONNECT FAIL"); break;
          default: Serial.println("UNKNOWN ERROR"); break;
        }
      }
      
      http.end();
    } else {
      Serial.println("Falha ao iniciar conexão HTTP");
    }
  } else {
    Serial.println("Não foi possível enviar dados - host Meshtastic inacessível");
    Serial.println("Verifique o endereço IP e a porta do nó Meshtastic nas configurações");
  }
}
#endif // USE_MESHTASTIC

// Configure deep sleep
void setupDeepSleep() {
  Serial.println("Configuring deep sleep...");
  
  // Get sleep time from config
  WeatherStationConfig* config = configManager.getConfig();
  
  // Configure external wake-up source (rain gauge interrupt)
  esp_sleep_enable_ext0_wakeup(RAIN_GAUGE_INTERRUPT_PIN, HIGH);
  Serial.println("External wake-up configured on pin " + String(RAIN_GAUGE_INTERRUPT_PIN));
  
  // Configure button wake-up (for configuration mode)
  esp_sleep_enable_ext1_wakeup(1ULL << CONFIG_BUTTON_PIN, ESP_EXT1_WAKEUP_ALL_LOW);
  Serial.println("Button wake-up configured on pin " + String(CONFIG_BUTTON_PIN));
  
  // Configure timer wake-up
  uint64_t sleepTime = config->deepSleepTimeMinutes * uS_TO_MIN_FACTOR;
  esp_sleep_enable_timer_wakeup(sleepTime);
  Serial.println("Timer wake-up configured for " + String(config->deepSleepTimeMinutes) + " minutes");
}

// Initialize the appropriate sensor based on build flags
void setupSensors() {
  #ifdef USE_DHT22
    Serial.println("Initializing DHT22 sensor...");
    dht.begin();
  #endif

  // Initialize I2C once for both AHT20 and BMP280 if needed
  #if defined(USE_AHT20) || defined(USE_BMP280)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  #endif

  #ifdef USE_AHT20
    Serial.println("Initializing AHT20 sensor...");
    if (!aht.begin()) {
      Serial.println("Could not find AHT20 sensor! Check wiring");
    } else {
      Serial.println("AHT20 sensor found");
    }
  #endif

  #ifdef USE_BMP280
    Serial.println("Initializing BMP280 sensor...");
    if (!bmp.begin(BMP280_ADDRESS)) {
      Serial.println("Could not find BMP280 sensor! Check wiring or try a different address");
    } else {
      // Default settings from the datasheet
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode
                      Adafruit_BMP280::SAMPLING_X2,     // Temp. oversampling
                      Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                      Adafruit_BMP280::FILTER_X16,      // Filtering
                      Adafruit_BMP280::STANDBY_MS_500); // Standby time
      Serial.println("BMP280 sensor found");
    }
  #endif
}

// Main function to read sensor data
bool readSensorData(float &temperature, float &humidity) {
  bool success = false;

  #ifdef USE_DHT22
    success = readDHT22(temperature, humidity);
  #endif

  // When both I2C sensors are used, prefer AHT20 for temperature/humidity
  // and BMP280 for additional pressure reading
  #if defined(USE_AHT20) && defined(USE_BMP280)
    success = readAHT20(temperature, humidity);
    // We'll read pressure from BMP280 separately when sending data
  #elif defined(USE_AHT20)
    success = readAHT20(temperature, humidity);
  #elif defined(USE_BMP280)
    success = readBMP280(temperature, humidity);
  #endif

  // If no sensor is defined, return false
  if (!success) {
    Serial.println("Failed to read from sensors or no sensors defined in build flags!");
  }
  
  return success;
}

#ifdef USE_AHT20
// Read temperature and humidity from AHT20 sensor
bool readAHT20(float &temperature, float &humidity) {
  Serial.println("Reading AHT20 sensor...");
  
  sensors_event_t humidityEvent, temperatureEvent;
  
  // Try reading a few times
  for (int i = 0; i < 3; i++) {
    if (aht.getEvent(&humidityEvent, &temperatureEvent)) {
      humidity = humidityEvent.relative_humidity;
      temperature = temperatureEvent.temperature;
      
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");
      return true;
    }
    
    Serial.println("Failed to read from AHT20 sensor, retrying...");
    delay(2000);
  }
  
  Serial.println("All attempts to read AHT20 sensor failed!");
  return false;
}
#endif

#ifdef USE_BMP280
// Read temperature and pressure from BMP280 sensor (BMP280 doesn't have humidity, use 0)
bool readBMP280(float &temperature, float &humidity) {
  Serial.println("Reading BMP280 sensor...");
  
  // Try reading a few times
  for (int i = 0; i < 3; i++) {
    temperature = bmp.readTemperature();
    // BMP280 doesn't measure humidity, so set humidity to 0
    humidity = 0;
    
    float pressure = bmp.readPressure() / 100.0F; // Convert Pa to hPa
    
    if (!isnan(temperature) && !isnan(pressure)) {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");
      Serial.print("Pressure: ");
      Serial.print(pressure);
      Serial.println(" hPa");
      Serial.println("Note: BMP280 does not have humidity sensor");
      return true;
    }
    
    Serial.println("Failed to read from BMP280 sensor, retrying...");
    delay(2000);
  }
  
  Serial.println("All attempts to read BMP280 sensor failed!");
  return false;
}
#endif

#ifdef USE_MQTT
// Function to send data via MQTT
bool sendDataToMQTT(float temperature, float humidity, float rainAmount) {
  Serial.println("Preparing to send data via MQTT...");
  
  // Get MQTT configuration
  WeatherStationConfig* config = configManager.getConfig();
  
  // Skip if server is not configured
  if (strlen(config->mqttServer) == 0) {
    Serial.println("MQTT server not configured, skipping");
    return false;
  }
  
  // Configure MQTT server
  mqttClient.setServer(config->mqttServer, config->mqttPort);
  
  // Generate a client ID if not configured
  String clientId = config->mqttClientId;
  if (clientId.length() == 0) {
    clientId = "ESP32Weather-";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    Serial.print("Generated MQTT client ID: ");
    Serial.println(clientId);
  }
  
  // Attempt to connect to MQTT broker
  Serial.print("Connecting to MQTT broker at ");
  Serial.print(config->mqttServer);
  Serial.print(":");
  Serial.println(config->mqttPort);
  
  // Try connecting with credentials if provided
  bool connected = false;
  if (strlen(config->mqttUsername) > 0) {
    Serial.println("Connecting with credentials");
    connected = mqttClient.connect(
      clientId.c_str(), 
      config->mqttUsername, 
      config->mqttPassword
    );
  } else {
    Serial.println("Connecting without credentials");
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (!connected) {
    Serial.print("Failed to connect to MQTT broker, error code: ");
    Serial.println(mqttClient.state());
    return false;
  }
  
  Serial.println("Connected to MQTT broker!");
  
  // Create JSON document for the weather data
  StaticJsonDocument<256> dataDoc;
  
  // Include temperature data
  dataDoc["temperature"] = round(temperature * 100) / 100;;
  
  // Include sensor-specific data
  #ifdef USE_DHT22
    dataDoc["humidity"] = humidity;
    dataDoc["sensor"] = "DHT22";
  #endif
  
  // When both I2C sensors are used, we get more complete data
  #if defined(USE_AHT20) && defined(USE_BMP280)
    dataDoc["humidity"] = round(humidity * 100) / 100; 
    float pressure = bmp.readPressure() / 100.0F; // Convert Pa to hPa
    dataDoc["pressure"] = round(pressure * 100) / 100;
    dataDoc["sensor"] = "AHT20+BMP280";
  #elif defined(USE_AHT20)
    dataDoc["humidity"] = humidity;
    dataDoc["sensor"] = "AHT20";
  #elif defined(USE_BMP280)
    dataDoc["pressure"] = bmp.readPressure() / 100.0F; // Convert Pa to hPa
    dataDoc["sensor"] = "BMP280";
  #endif
  
  // Include rain data and node identification
  dataDoc["rain"] = rainAmount;
  dataDoc["rain_1h"] = rainLastHour;
  dataDoc["rain_24h"] = rainLast24Hours;
  dataDoc["node_name"] = config->deviceName;
  
  // Adiciona apenas timestamp NTP (Unix) se disponível
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    time_t currentTime = getLocalTime();
    dataDoc["timestamp"] = currentTime;  // Timestamp Unix (segundos desde 1970)
  }
  
  // add battery voltage
  dataDoc["voltage"] = getBatteryVoltage();
  dataDoc["BatteryLevel"] = batteryLevel(getBatteryVoltage());
  
  // Serialize weather data JSON to string
  String dataString;
  serializeJson(dataDoc, dataString);
  
  // Use topic from configuration or default
  String topic = config->mqttTopic;
  if (topic.length() == 0) {
    topic = "esp32/weather/";
    topic += config->deviceName;
  }
  
  Serial.print("Publishing to topic: ");
  Serial.println(topic);
  Serial.print("Data: ");
  Serial.println(dataString);
  
  // Publish data to the MQTT topic
  bool published = mqttClient.publish(topic.c_str(), dataString.c_str(), true);
  
  if (published) {
    Serial.println("Data published successfully");
    
    // Check if interval updates are enabled
    /* if (config->mqttUpdateInterval > 0) {
      Serial.print("MQTT update interval is set to ");
      Serial.print(config->mqttUpdateInterval);
      Serial.println(" seconds");
      Serial.println("Waiting for additional update period before sleep...");
      
      // Calculate maximum time to spend in update loop
      unsigned long maxUpdateTime = min((unsigned long)(config->mqttUpdateInterval * 1000), 
                                       (unsigned long)(MAX_RUNTIME_MS - (millis() - startTime)));
      
      unsigned long updateStart = millis();
      unsigned long lastPublish = millis();
      
      while (millis() - updateStart < maxUpdateTime) {
        // Check if we need to publish again
        if (millis() - lastPublish >= config->mqttUpdateInterval * 1000) {
          // Read sensor data again
          float newTemp = 0.0;
          float newHumidity = 0.0;
          
          if (readSensorData(newTemp, newHumidity)) {
            // Update temperature, humidity and rainfall data
            dataDoc["temperature"] = round(newTemp * 100) / 100;
            dataDoc["humidity"] = round(newHumidity * 100) / 100;
            dataDoc["rain_1h"] = rainLastHour;
            dataDoc["rain_24h"] = rainLast24Hours;
            dataDoc["uptime"] = millis() / 1000;
            
            // Serialize and publish
            serializeJson(dataDoc, dataString);
            Serial.print("Publishing update: ");
            Serial.println(dataString);
            
            if (mqttClient.publish(topic.c_str(), dataString.c_str(), true)) {
              Serial.println("Update published successfully");
            } else {
              Serial.println("Failed to publish update");
            }
          }
          
          lastPublish = millis();
        }
        
        // Process MQTT messages
        mqttClient.loop();
        delay(100);
        
        // Check if runtime is getting too long
        if (shouldEnterSleep()) {
          Serial.println("Update loop interrupted due to maximum runtime");
          break;
        }
      }
    } */
    
    // Disconnect MQTT client
    mqttClient.disconnect();
    return true;
  } else {
    Serial.println("Failed to publish data");
    mqttClient.disconnect();
    return false;
  }
}
#endif // USE_MQTT

// Adiciona um novo registro de chuva ao histórico
void addRainRecord(float amount) {
  // Se não houver nenhuma chuva, não registre
  if (amount <= 0) {
    return;
  }
  
  // Tenta obter o timestamp UTC do NTP se possível
  time_t currentTime;
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    // Usar timestamp NTP se disponível
    currentTime = getLocalTime();
    Serial.println("Usando timestamp NTP para registro de chuva");
  } else {
    // Fallback para millis se NTP não disponível
    currentTime = millis() / 1000;
    
    // Evite overflow do millis() reiniciando o tempo base se necessário
    if (lastResetTime == 0) {
      lastResetTime = currentTime;
    }
    
    Serial.println("NTP não disponível, usando timestamp local relativo");
  }
  
  // Se o buffer estiver cheio, remova o registro mais antigo
  if (rainHistoryCount >= MAX_RAIN_RECORDS) {
    // Desloca todos os registros uma posição para trás
    for (int i = 0; i < rainHistoryCount - 1; i++) {
      rainHistory[i] = rainHistory[i + 1];
    }
    rainHistoryCount--;
  }
  
  // Adiciona o novo registro
  rainHistory[rainHistoryCount].timestamp = currentTime;
  rainHistory[rainHistoryCount].amount = amount;
  rainHistoryCount++;
  
  // Atualiza o total de chuva
  totalRainfall += amount;
  
  Serial.print("Registro de chuva adicionado: ");
  Serial.print(amount);
  Serial.print(" mm no timestamp ");
  Serial.println(currentTime);
  
  // Se temos NTP, vamos mostrar o horário legível
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    struct tm timeinfo;
    if(gmtime_r(&currentTime, &timeinfo)) {
      char timeStr[30];
      strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
      Serial.print("Data/hora: ");
      Serial.println(timeStr);
    }
  }
  Serial.print("Total acumulado: ");
  Serial.print(totalRainfall);
  Serial.println(" mm");
}

// Calcula a quantidade de chuva na última hora
float getRainLastHour() {
  float rainLastHour = 0.0;
  
  // Tenta usar o timestamp NTP se disponível
  time_t currentTime;
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    currentTime = getLocalTime();
  } else {
    currentTime = millis() / 1000;
  }
  
  time_t oneHourAgo = currentTime - (HOUR_MILLIS / 1000);
  
  /* Serial.print("Calculando chuva na última hora. Tempo atual: ");
  Serial.print(currentTime);
  Serial.print(", uma hora atrás: ");
  Serial.println(oneHourAgo); */
  
  // Se temos NTP, vamos mostrar o horário legível
 /*  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    struct tm nowInfo, hourAgoInfo;
    if(gmtime_r(&currentTime, &nowInfo) && gmtime_r(&oneHourAgo, &hourAgoInfo)) {
      char nowStr[30], hourAgoStr[30];
      strftime(nowStr, sizeof(nowStr), "%d/%m/%Y %H:%M:%S", &nowInfo);
      strftime(hourAgoStr, sizeof(hourAgoStr), "%d/%m/%Y %H:%M:%S", &hourAgoInfo);
      Serial.print("Data/hora atual: ");
      Serial.print(nowStr);
      Serial.print("  Uma hora atrás: ");
      Serial.println(hourAgoStr);
    }
  } */
  
  for (int i = 0; i < rainHistoryCount; i++) {
    // Se o registro for mais recente que uma hora atrás
    if (rainHistory[i].timestamp >= oneHourAgo) {
      rainLastHour += rainHistory[i].amount;
    }
  }
  
  Serial.print("Chuva na última hora: ");
  Serial.print(rainLastHour);
  Serial.println(" mm");
  
  return rainLastHour;
}

// Calcula a quantidade de chuva nas últimas 24 horas
float getRainLast24Hours() {
  float rainLast24Hours = 0.0;
  
  // Tenta usar o timestamp NTP se disponível
  time_t currentTime;
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    currentTime = getLocalTime();
  } else {
    currentTime = millis() / 1000;
  }
  
  time_t oneDayAgo = currentTime - (DAY_MILLIS / 1000);
  
  /* Serial.print("Calculando chuva nas últimas 24 horas. Tempo atual: ");
  Serial.print(currentTime);
  Serial.print(", 24 horas atrás: ");
  Serial.println(oneDayAgo); */
  
  // Se temos NTP, vamos mostrar o horário legível
  /* if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    struct tm nowInfo, dayAgoInfo;
    if(gmtime_r(&currentTime, &nowInfo) && gmtime_r(&oneDayAgo, &dayAgoInfo)) {
      char nowStr[30], dayAgoStr[30];
      strftime(nowStr, sizeof(nowStr), "%d/%m/%Y %H:%M:%S", &nowInfo);
      strftime(dayAgoStr, sizeof(dayAgoStr), "%d/%m/%Y %H:%M:%S", &dayAgoInfo);
      Serial.print("Data/hora atual: ");
      Serial.print(nowStr);
      Serial.print("  24 horas atrás: ");
      Serial.println(dayAgoStr);
    }
  } */
  
  for (int i = 0; i < rainHistoryCount; i++) {
    // Se o registro for mais recente que 24 horas atrás
    if (rainHistory[i].timestamp >= oneDayAgo) {
      rainLast24Hours += rainHistory[i].amount;
    }
  }
  
  Serial.print("Chuva nas últimas 24 horas: ");
  Serial.print(rainLast24Hours);
  Serial.println(" mm");
  
  return rainLast24Hours;
}

// Gerencia o histórico de registros de chuva - limpa registros muito antigos
void manageRainHistory() {
  // Tenta usar o timestamp NTP se disponível
  time_t currentTime;
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    currentTime = getLocalTime();
  } else {
    currentTime = millis() / 1000;
  }
  
  time_t oneDayAgo = currentTime - (DAY_MILLIS / 1000);
  int recordsToKeep = 0;
  
  Serial.println("Gerenciando histórico de chuva...");
  
  // Se temos NTP, vamos mostrar o horário legível
  if (WiFi.status() == WL_CONNECTED && lastNTPSync > 0) {
    struct tm timeinfo;
    if(gmtime_r(&currentTime, &timeinfo)) {
      char timeStr[30];
      strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
      Serial.print("Data/hora atual: ");
      Serial.println(timeStr);
    }
  }
  
  // Conta quantos registros são mais recentes que 24 horas
  for (int i = 0; i < rainHistoryCount; i++) {
    if (rainHistory[i].timestamp >= oneDayAgo) {
      // Se encontramos um registro no intervalo, move ele para o início do array
      if (i > recordsToKeep) {
        rainHistory[recordsToKeep] = rainHistory[i];
      }
      recordsToKeep++;
    }
  }
  
  // Atualiza o contador para refletir apenas os registros mantidos
  if (rainHistoryCount != recordsToKeep) {
    Serial.print("Limpando histórico de chuva: ");
    Serial.print(rainHistoryCount - recordsToKeep);
    Serial.println(" registros antigos removidos");
    rainHistoryCount = recordsToKeep;
  }
}

float getBatteryVoltage(){
  // float voltage = analogRead(BATTERY_ADC_PIN) * (3.3 / 4095.0);
  int raw = analogRead(BATTERY_ADC_PIN);
  float voltage = ((float)raw / 4095.0) * 3.6 * 2.0;
  return voltage;
}

int batteryLevel(float voltage) {
  if (voltage >= 4.2) return 100;
  if (voltage >= 3.95) return 75;
  if (voltage >= 3.7) return 50;
  if (voltage >= 3.5) return 25;
  return 10;  // Considerado nível crítico
}

// Sincroniza o relógio com servidores NTP
void syncTimeWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não conectado, impossível sincronizar com NTP");
    return;
  }
  
  // Verificar se precisamos sincronizar
  time_t now = time(nullptr);
  if (lastNTPSync > 0 && now > lastNTPSync && (now - lastNTPSync < NTP_SYNC_INTERVAL / 1000)) {
    Serial.println("Sincronização NTP recente, pulando...");
    return;
  }
  
  Serial.println("Configurando servidores NTP...");
  configTime(NTP_TIMEZONE * 3600, 0, NTP_SERVER1, NTP_SERVER2);
  
  // Aguardar sincronização
  Serial.println("Aguardando sincronização NTP...");
  time_t startWait = millis();
  time_t timeout = 10000; // 10 segundos de timeout
  
  while (time(nullptr) < 1609459200) { // 1 de janeiro de 2021 como referência
    delay(100);
    if (millis() - startWait > timeout) {
      Serial.println("Timeout na sincronização NTP");
      return;
    }
  }
  
  now = time(nullptr);
  lastNTPSync = now;
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    Serial.print("Horário sincronizado: ");
    Serial.println(buffer);
  } else {
    Serial.println("Falha ao obter horário local");
  }
}

// Retorna o tempo local atual em timestamp Unix
time_t getLocalTime() {
  if (WiFi.status() == WL_CONNECTED && (lastNTPSync == 0 || time(nullptr) - lastNTPSync >= NTP_SYNC_INTERVAL / 1000)) {
    syncTimeWithNTP();
  }
  return time(nullptr);
}
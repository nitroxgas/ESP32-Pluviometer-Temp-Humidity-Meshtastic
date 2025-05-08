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
#include "config.h"
#include "ConfigManager.h"

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

// Define RTC variables that persist through deep sleep
RTC_DATA_ATTR int rainCounter = 0;    // Rain counter
RTC_DATA_ATTR bool isFirstRun = true; // Flag for first run after power-on
RTC_DATA_ATTR bool needsConfiguration = false; // Flag to enter configuration mode

// Define wake-up sources
#define TIMER_WAKEUP 1
#define EXTERNAL_WAKEUP 2
#define BUTTON_WAKEUP 3
RTC_DATA_ATTR int wakeupReason = 0;

// Variables for runtime management
unsigned long startTime; // To track how long the device has been running

// Function prototypes
void setupWiFi();
void setupSensors();
bool readSensorData(float &temperature, float &humidity);

#ifdef USE_DHT22
bool readDHT22(float &temperature, float &humidity);
#endif

#ifdef USE_AHT20
bool readAHT20(float &temperature, float &humidity);
#endif

#ifdef USE_BMP280
bool readBMP280(float &temperature, float &humidity);
#endif

void sendDataToMeshtastic(float temperature, float humidity, float rainAmount);
void printWakeupReason();
void setupDeepSleep();
void setCpuFrequency();
bool shouldEnterSleep();

void setup() {
  // Record the start time
  startTime = millis();
  
  // Initialize serial communication
  Serial.begin(115200);
  
  // Short delay to allow serial to initialize
  delay(1000);
  
  Serial.println("\n\nESP32 Weather Station Starting...");
  
  // Initialize ConfigManager
  if (!configManager.begin()) {
    Serial.println("Failed to initialize ConfigManager!");
    // Continue with default values
  }
  
  // Determine wake-up reason
  printWakeupReason();
  
  // Get current configuration
  WeatherStationConfig* config = configManager.getConfig();
  
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
  if (wakeupReason == EXTERNAL_WAKEUP) {
    // If woken by interrupt, increment rain counter
    rainCounter++;
    Serial.println("Rain detected! Counter: " + String(rainCounter));
  }
  
  // Calculate total rain amount with calibration from config
  rainAmount = rainCounter * config->rainMmPerTip;
  
  // Connect to WiFi
  setupWiFi();
  
  // Check if we should enter sleep
  if (shouldEnterSleep()) {
    setupDeepSleep();
    Serial.println("Maximum runtime exceeded after WiFi connection, entering deep sleep...");
    esp_deep_sleep_start();
    return; // This will never be reached
  }
  
  // Send data to Meshtastic node
  if (WiFi.status() == WL_CONNECTED) {
    sendDataToMeshtastic(temperature, humidity, rainAmount);
    
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

// Send data to Meshtastic node using the toRadio API endpoint with correct protobuf structure
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
  dataDoc["node_name"] = config->deviceName;
  
  // Serialize weather data JSON to string
  String dataString;
  serializeJson(dataDoc, dataString);
  
  Serial.print("Weather data: ");
  Serial.println(dataString);
  
  // Create the ToRadio message structure according to Meshtastic protobufs
  StaticJsonDocument<1024> toRadioDoc;
  
  // Create a meshPacket object (this will be nested within the toRadio message)
  JsonObject packetObj = toRadioDoc.createNestedObject("packet");
  
  // Fill in MeshPacket fields according to the protobuf definition
  packetObj["from"] = 0;                                // Our device ID (0 means it should get our node number)
  packetObj["to"] = BROADCAST_ADDR;                     // Broadcast to all nodes (BROADCAST_ADDR = 0xffffffff)
  packetObj["id"] = random(0, 1000000);                 // Random packet ID
  packetObj["want_ack"] = false;                        // No acknowledgment needed 
  packetObj["priority"] = "RELIABLE";                   // Priority for the packet
  
  // Create the payload object within the packet
  JsonObject payloadObj = packetObj.createNestedObject("decoded");
  payloadObj["portnum"] = "TEXT_MESSAGE_APP";           // Use text message app (port 1)
  payloadObj["payload"] = dataString;                   // The weather data as string payload
  
  // Serialize the ToRadio JSON
  String toRadioJson;
  serializeJson(toRadioDoc, toRadioJson);
  
  // Send data to Meshtastic node
  Serial.println("Sending data to Meshtastic node...");
  
  HTTPClient http;
  String url = "http://" + String(config->meshtasticNodeIP) + ":" + 
               String(config->meshtasticNodePort) + MESHTASTIC_API_ENDPOINT;
  
  Serial.print("Sending to URL: ");
  Serial.println(url);
  Serial.print("ToRadio payload: ");
  Serial.println(toRadioJson);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // The toRadio endpoint uses PUT request
  int httpResponseCode = http.PUT(toRadioJson);
  
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
  }
  
  http.end();
}

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
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
#include <DHT.h>
#include <ArduinoJson.h>
#include "config.h"

// Initialize DHT sensor
DHT dht(DHT_PIN, DHT22);

// Define RTC variables that persist through deep sleep
RTC_DATA_ATTR int rainCounter = 0;    // Rain counter
RTC_DATA_ATTR bool isFirstRun = true; // Flag for first run after power-on

// Define wake-up sources
#define TIMER_WAKEUP 1
#define EXTERNAL_WAKEUP 2
RTC_DATA_ATTR int wakeupReason = 0;

// Variables for runtime management
unsigned long startTime; // To track how long the device has been running

// Function prototypes
void setupWiFi();
bool readDHT22(float &temperature, float &humidity);
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
  
  // Set CPU frequency to 160MHz
  setCpuFrequency();
  
  // Determine wake-up reason
  printWakeupReason();
  
  // If this is the first run after power-on, initialize rain counter
  if (isFirstRun) {
    Serial.println("First run after power-on, initializing rain counter");
    rainCounter = 0;
    isFirstRun = false;
  }
  
  // Initialize DHT sensor
  dht.begin();
  
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
  bool dhtReadSuccess = readDHT22(temperature, humidity);
  
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
  
  // Calculate total rain amount
  rainAmount = rainCounter * RAIN_MM_PER_TIP;
  
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

// Set CPU frequency to value defined in config.h (default 160MHz)
void setCpuFrequency() {
  Serial.print("Setting CPU frequency to ");
  Serial.print(CPU_FREQ_MHZ);
  Serial.print("MHz...");
  if (setCpuFrequencyMhz(CPU_FREQ_MHZ)) {
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
      Serial.println("Wakeup caused by external signal using RTC_IO");
      wakeupReason = EXTERNAL_WAKEUP;
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
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
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
    Serial.println("Connection failed!");
  }
}

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

// Send data to Meshtastic node
void sendDataToMeshtastic(float temperature, float humidity, float rainAmount) {
  Serial.println("Preparing data for Meshtastic node...");
  
  // Create JSON document
  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["rain"] = rainAmount;
  
  // Serialize JSON to string
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("JSON data: ");
  Serial.println(jsonString);
  
  // Send data to Meshtastic node
  Serial.println("Sending data to Meshtastic node...");
  
  HTTPClient http;
  String url = "http://" + String(MESHTASTIC_NODE_IP) + ":" + 
               String(MESHTASTIC_NODE_PORT) + MESHTASTIC_API_ENDPOINT;
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Response: " + payload);
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

// Configure deep sleep
void setupDeepSleep() {
  Serial.println("Configuring deep sleep...");
  
  // Configure external wake-up source (rain gauge interrupt)
  esp_sleep_enable_ext0_wakeup(RAIN_GAUGE_INTERRUPT_PIN, HIGH);
  Serial.println("External wake-up configured on pin " + String(RAIN_GAUGE_INTERRUPT_PIN));
  
  // Configure timer wake-up
  uint64_t sleepTime = DEEP_SLEEP_TIME_MINUTES * uS_TO_MIN_FACTOR;
  esp_sleep_enable_timer_wakeup(sleepTime);
  Serial.println("Timer wake-up configured for " + String(DEEP_SLEEP_TIME_MINUTES) + " minutes");
}

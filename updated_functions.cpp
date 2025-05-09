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
  
  // Serialize weather data JSON to string
  String dataString;
  serializeJson(dataDoc, dataString);
  
  Serial.print("Weather data: ");
  Serial.println(dataString);
}
#endif // USE_MESHTASTIC

#ifdef USE_MQTT
// Function to send data via MQTT
bool sendDataToMQTT(float temperature, float humidity, float rainAmount) {
  // Código até a parte do JSON...
  
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
}
#endif // USE_MQTT
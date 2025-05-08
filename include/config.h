#ifndef CONFIG_H
#define CONFIG_H

// Valores padrão para configurações ajustáveis - podem ser alterados via BLE/Web
#define DEFAULT_DEEP_SLEEP_TIME_MINUTES 5    // Deep sleep duration in minutes
#define DEFAULT_CPU_FREQ_MHZ 160             // CPU frequency in MHz (80 or 160 for ESP32)
#define DEFAULT_RAIN_MM_PER_TIP 0.25         // Rain gauge produces 0.25mm per tip/interrupt
#define DEFAULT_WIFI_SSID "GeorgeHome"        // WiFi SSID
#define DEFAULT_WIFI_PASSWORD "Cz1mwyh." // WiFi password
#define DEFAULT_MESHTASTIC_NODE_IP "192.168.1.98"  // IP address of Meshtastic node
#define DEFAULT_MESHTASTIC_NODE_PORT 80             // Port of Meshtastic node
#define DEFAULT_DEVICE_NAME "ESP32-Weather"        // Nome do dispositivo para BLE

// Configurações fixas do sistema
#define MAX_RUNTIME_MS 30000         // Maximum runtime before forced sleep (30 seconds)
#define uS_TO_MIN_FACTOR 60000000ULL // Conversion factor: microseconds to minutes
#define WIFI_TIMEOUT 20000           // WiFi connection timeout in milliseconds
#define MESHTASTIC_API_ENDPOINT "/api/v1/sendtext"  // Endpoint for sending data
#define CONFIG_AP_PASSWORD "weatherconfig"  // Senha do ponto de acesso no modo configuração
#define CONFIG_PORTAL_TIMEOUT 180    // Tempo limite (em segundos) do portal de configuração

// Rain gauge configuration (interrupt)
#define RAIN_GAUGE_INTERRUPT_PIN GPIO_NUM_27 // Pin connected to rain gauge interrupt

// DHT22 pin definition
#ifdef USE_DHT22
    #define DHT_PIN 4                  // DHT22 data pin
#endif

// I2C pin definitions (for AHT20 and BMP280)
#if defined(USE_AHT20) || defined(USE_BMP280)
    #define I2C_SDA_PIN 21             // I2C SDA pin
    #define I2C_SCL_PIN 22             // I2C SCL pin
#endif

#ifdef USE_BMP280
    #define BMP280_ADDRESS 0x76        // Default BMP280 I2C address (some modules use 0x77)
#endif

// Configuração BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CONFIG_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Configuração do modo de configuração
#define CONFIG_BUTTON_PIN GPIO_NUM_0  // Botão BOOT do ESP32 para entrar no modo de configuração

// Debug configuration
#define DEBUG_ENABLED true     // Enable/disable debug output

#endif // CONFIG_H
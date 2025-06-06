#ifndef CONFIG_H
#define CONFIG_H

// Valores padrão para configurações ajustáveis - podem ser alterados via BLE/Web
#define DEFAULT_DEEP_SLEEP_TIME_MINUTES 5    // Deep sleep duration in minutes
#define DEFAULT_CPU_FREQ_MHZ 160             // CPU frequency in MHz (80 or 160 for ESP32)
#define DEFAULT_RAIN_MM_PER_TIP 0.25         // Rain gauge produces 0.25mm per tip/interrupt

// Configurações para histórico de precipitação
#define MAX_RAIN_RECORDS 288                 // Registros para 24 horas (suponha um registro a cada 5 minutos)
#define HOUR_MILLIS 3600000UL                // Milissegundos em uma hora
#define DAY_MILLIS 86400000UL                // Milissegundos em um dia (24 horas)
#define DEFAULT_WIFI_SSID "your_wifi_ssid"        // WiFi SSID
#define DEFAULT_WIFI_PASSWORD "your_wifi_password" // WiFi password
#define DEFAULT_DEVICE_NAME "ESP32-Weather"        // Nome do dispositivo para BLE

// Configurações Meshtastic
#define DEFAULT_MESHTASTIC_NODE_IP "192.168.1.100"  // IP address of Meshtastic node
#define DEFAULT_MESHTASTIC_NODE_PORT 80             // Port of Meshtastic node

// Configurações MQTT
#define DEFAULT_MQTT_SERVER "mqtt.example.com"  // Servidor MQTT
#define DEFAULT_MQTT_PORT 1883                  // Porta do servidor MQTT
#define DEFAULT_MQTT_USERNAME ""                // Nome de usuário MQTT (opcional)
#define DEFAULT_MQTT_PASSWORD ""                // Senha MQTT (opcional)
#define DEFAULT_MQTT_CLIENT_ID "esp32weather"   // ID do cliente MQTT
#define DEFAULT_MQTT_TOPIC "weather/station1"   // Tópico para publicação
#define DEFAULT_MQTT_UPDATE_INTERVAL 0          // Intervalo em segundos (0 = único envio)

// Configurações do NTP (Network Time Protocol)
#define NTP_SERVER1 "pool.ntp.org"              // Servidor NTP primário
#define NTP_SERVER2 "time.nist.gov"             // Servidor NTP secundário
#define NTP_TIMEZONE -3                         // Fuso horário em horas (exemplo: -3 para Brasília)
#define NTP_SYNC_INTERVAL 3600000               // Intervalo de sincronização em ms (1 hora)

// Configurações fixas do sistema
#define MAX_RUNTIME_MS 30000         // Maximum runtime before forced sleep (30 seconds)
#define uS_TO_MIN_FACTOR 60000000ULL // Conversion factor: microseconds to minutes
#define WIFI_TIMEOUT 20000           // WiFi connection timeout in milliseconds
#define MESHTASTIC_API_ENDPOINT "/api/v1/toradio"  // Endpoint for sending messages to radio
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
#define BATTERY_ADC_PIN 35     // Pino ADC para medição de tensão da bateria

// Debug configuration
#define DEBUG_ENABLED true     // Enable/disable debug output

// Meshtastic specific constants
#define BROADCAST_ADDR 0xffffffff  // Broadcast address for Meshtastic nodes

#endif // CONFIG_H
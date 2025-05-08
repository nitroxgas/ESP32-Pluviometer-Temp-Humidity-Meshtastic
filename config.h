#ifndef CONFIG_H
#define CONFIG_H

// Sleep configuration
#define DEEP_SLEEP_TIME_MINUTES 5  // Time to sleep between readings in minutes
#define uS_TO_MIN_FACTOR 60000000  // Conversion factor from microseconds to minutes
#define MAX_RUNTIME_MS 30000       // Maximum runtime before going to sleep (30 seconds)

// Pin definitions
#define DHT_PIN 4                  // DHT22 data pin
#define RAIN_GAUGE_INTERRUPT_PIN 27 // Rain gauge interrupt pin (GPIO27)

// WiFi configuration
#define WIFI_SSID "your_wifi_ssid"        // WiFi SSID
#define WIFI_PASSWORD "your_wifi_password" // WiFi password
#define WIFI_TIMEOUT 20000                // WiFi connection timeout in milliseconds

// Meshtastic node configuration
#define MESHTASTIC_NODE_IP "192.168.1.100"  // IP address of Meshtastic node
#define MESHTASTIC_NODE_PORT 80             // Port of Meshtastic node
#define MESHTASTIC_API_ENDPOINT "/api/v1/sendtext"  // Endpoint for sending data

// Rain gauge configuration
#define RAIN_MM_PER_TIP 0.25  // Rain gauge produces 0.25mm per tip/interrupt

// CPU configuration
#define CPU_FREQ_MHZ 160      // CPU frequency in MHz (80 or 160 for ESP32)

// Debug configuration
#define DEBUG_ENABLED true     // Enable/disable debug output

#endif // CONFIG_H

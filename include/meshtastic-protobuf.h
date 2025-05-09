#ifndef MESHTASTIC_PROTOBUF_H
#define MESHTASTIC_PROTOBUF_H

#include <stdint.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <ArduinoJson.h>

// Definições chave do Meshtastic
#define BROADCAST_ADDR 0xffffffff
#define MAX_DATA_PAYLOAD_SIZE 240

// Constantes para identificadores de portas (portnum) conforme protobuf do Meshtastic
enum PortNum {
  UNKNOWN_APP = 0,
  TEXT_MESSAGE_APP = 1,  // Aplicativo de mensagens de texto simples
  REMOTE_HARDWARE_APP = 2,
  POSITION_APP = 3,
  NODEINFO_APP = 4,
  ROUTING_APP = 5,
  ADMIN_APP = 6,
  SENSOR_APP = 7,      // Adicionando SENSOR_APP para dados de sensores
  WEATHER_APP = 8      // Adicionando um app específico para dados meteorológicos
};

// Tipos de prioridade de pacotes do Meshtastic
enum Priority {
  UNSET = 0,
  MIN = 1,
  BACKGROUND = 10,
  NORMAL = 64,      // Renomeado de DEFAULT para NORMAL para evitar conflito
  RELIABLE = 70,
  ACK = 190,
  MAX = 200
};

// Estrutura simplificada para DataPacket 
// Representa o payload do protobuf real
typedef struct {
  char data[MAX_DATA_PAYLOAD_SIZE];  // Dados da mensagem em formato texto
  uint8_t size;                     // Tamanho dos dados
} DataPacket;

// Estrutura simplificada que simula parte do MeshPacket
typedef struct {
  uint32_t from;       // ID do remetente
  uint32_t to;         // ID do destinatário
  uint32_t id;         // ID da mensagem
  bool want_ack;       // Solicitar confirmação
  uint8_t port;        // Porta (portnum)
  Priority priority;   // Prioridade da mensagem
  DataPacket payload;  // Carga útil
} MeshPacket;

// Estrutura simplificada para ToRadio
typedef struct {
  MeshPacket packet;  // Pacote a ser enviado
} ToRadio;

// Converte uma string de enum para seu valor numério
uint8_t portnumFromString(const char* portText) {
  if (strcmp(portText, "TEXT_MESSAGE_APP") == 0) return TEXT_MESSAGE_APP;
  if (strcmp(portText, "REMOTE_HARDWARE_APP") == 0) return REMOTE_HARDWARE_APP;
  if (strcmp(portText, "POSITION_APP") == 0) return POSITION_APP;
  if (strcmp(portText, "NODEINFO_APP") == 0) return NODEINFO_APP;
  if (strcmp(portText, "ROUTING_APP") == 0) return ROUTING_APP;
  if (strcmp(portText, "ADMIN_APP") == 0) return ADMIN_APP;
  if (strcmp(portText, "SENSOR_APP") == 0) return SENSOR_APP;
  if (strcmp(portText, "WEATHER_APP") == 0) return WEATHER_APP;
  return TEXT_MESSAGE_APP; // Valor padrão
}

// Converte um valor numérico de porta para sua representação em string
const char* portnumToString(uint8_t portnum) {
  switch (portnum) {
    case TEXT_MESSAGE_APP: return "TEXT_MESSAGE_APP";
    case REMOTE_HARDWARE_APP: return "REMOTE_HARDWARE_APP";
    case POSITION_APP: return "POSITION_APP";
    case NODEINFO_APP: return "NODEINFO_APP";
    case ROUTING_APP: return "ROUTING_APP";
    case ADMIN_APP: return "ADMIN_APP";
    case SENSOR_APP: return "SENSOR_APP";
    case WEATHER_APP: return "WEATHER_APP";
    default: return "TEXT_MESSAGE_APP";
  }
}

// Converte uma prioridade em string para seu valor numérico
Priority priorityFromString(const char* prioText) {
  if (strcmp(prioText, "MIN") == 0) return MIN;
  if (strcmp(prioText, "BACKGROUND") == 0) return BACKGROUND;
  if (strcmp(prioText, "NORMAL") == 0) return NORMAL;
  if (strcmp(prioText, "DEFAULT") == 0) return NORMAL;  // Para compatibilidade com código existente
  if (strcmp(prioText, "RELIABLE") == 0) return RELIABLE;
  if (strcmp(prioText, "ACK") == 0) return ACK;
  if (strcmp(prioText, "MAX") == 0) return MAX;
  return NORMAL; // Valor padrão
}

// Converte um valor numérico de prioridade para string
const char* priorityToString(Priority priority) {
  switch (priority) {
    case MIN: return "MIN";
    case BACKGROUND: return "BACKGROUND";
    case NORMAL: return "NORMAL";
    case RELIABLE: return "RELIABLE";
    case ACK: return "ACK";
    case MAX: return "MAX";
    default: return "NORMAL";
  }
}

// Função para converter MeshPacket para formato JSON para API HTTP do Meshtastic
String createMeshtasticToRadioJson(const MeshPacket& packet) {
  StaticJsonDocument<1536> toRadioDoc;
  
  // Criar o objeto packet
  JsonObject packetObj = toRadioDoc.createNestedObject("packet");
  
  // Preencher campos do MeshPacket
  packetObj["from"] = packet.from;
  packetObj["to"] = packet.to;
  packetObj["id"] = packet.id;
  packetObj["want_ack"] = packet.want_ack;
  packetObj["priority"] = priorityToString(packet.priority);
  
  // Criar payload decodificado
  JsonObject decodedObj = packetObj.createNestedObject("decoded");
  decodedObj["portnum"] = portnumToString(packet.port);
  
  // Adicionar a carga útil (payload)
  decodedObj["payload"] = packet.payload.data;
  
  // Serializar em JSON
  String jsonOutput;
  serializeJson(toRadioDoc, jsonOutput);
  
  // Depuração
  Serial.print("Pacote convertido para JSON: ");
  Serial.println(jsonOutput);
  
  return jsonOutput;
}

// Função para criar um pacote MeshPacket com dados meteorológicos
MeshPacket createWeatherDataPacket(const String& jsonData, uint32_t nodeNum) {
  MeshPacket packet;
  
  // Configurar cabeçalho do pacote
  packet.from = nodeNum;
  packet.to = BROADCAST_ADDR;
  packet.id = random(1, 1000000);
  packet.want_ack = false;
  packet.port = TEXT_MESSAGE_APP;  // Usar porta específica para dados meteorológicos
  packet.priority = RELIABLE; // Usar prioridade confiável
  
  // Configurar payload
  if (jsonData.length() < sizeof(packet.payload.data)) {
    strncpy(packet.payload.data, jsonData.c_str(), sizeof(packet.payload.data) - 1);
    packet.payload.data[sizeof(packet.payload.data) - 1] = '\0';
    packet.payload.size = jsonData.length();
  } else {
    Serial.println("AVISO: Dados muito grandes para o payload, truncando!");
    strncpy(packet.payload.data, jsonData.c_str(), sizeof(packet.payload.data) - 1);
    packet.payload.data[sizeof(packet.payload.data) - 1] = '\0';
    packet.payload.size = sizeof(packet.payload.data) - 1;
  }
  
  return packet;
}

#endif // MESHTASTIC_PROTOBUF_H
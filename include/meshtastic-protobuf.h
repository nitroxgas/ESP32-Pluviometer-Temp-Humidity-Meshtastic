#ifndef MESHTASTIC_PROTOBUF_H
#define MESHTASTIC_PROTOBUF_H

#include <stdint.h>
#include <pb_encode.h>
#include <pb_decode.h>

// Definições chave do Meshtastic
#define BROADCAST_ADDR 0xffffffff

// Constantes para identificadores de portas (portnum) conforme protobuf do Meshtastic
enum PortNum {
  UNKNOWN_APP = 0,
  TEXT_MESSAGE_APP = 1,  // Aplicativo de mensagens de texto simples
  REMOTE_HARDWARE_APP = 2,
  POSITION_APP = 3,
  NODEINFO_APP = 4,
  ROUTING_APP = 5,
  ADMIN_APP = 6
};

// Estrutura simplificada para DataPacket 
// Seria o conteúdo do campo "payload" no protobuf real
typedef struct {
  char data[256];  // Dados da mensagem em formato texto
  uint8_t size;    // Tamanho dos dados
} DataPacket;

// Estrutura simplificada que simula parte do MeshPacket
typedef struct {
  uint32_t from;       // ID do remetente
  uint32_t to;         // ID do destinatário
  uint32_t id;         // ID da mensagem
  bool want_ack;       // Solicitar confirmação
  uint8_t port;        // Porta (portnum)
  DataPacket payload;  // Carga útil
} MeshPacket;

// Estrutura simplificada para ToRadio
typedef struct {
  MeshPacket packet;  // Pacote a ser enviado
} ToRadio;

// Função para converter DataPacket para formato JSON para API HTTP do Meshtastic
String createMeshtasticToRadioJson(const MeshPacket& packet) {
  StaticJsonDocument<1024> toRadioDoc;
  
  // Criar o objeto packet
  JsonObject packetObj = toRadioDoc.createNestedObject("packet");
  
  // Preencher campos do MeshPacket
  packetObj["from"] = packet.from;
  packetObj["to"] = packet.to;
  packetObj["id"] = packet.id;
  packetObj["want_ack"] = packet.want_ack;
  
  // Criar payload decodificado
  JsonObject decodedObj = packetObj.createNestedObject("decoded");
  
  // Determinar portnum como texto
  switch (packet.port) {
    case TEXT_MESSAGE_APP:
      decodedObj["portnum"] = "TEXT_MESSAGE_APP";
      break;
    case POSITION_APP:
      decodedObj["portnum"] = "POSITION_APP";
      break;
    case NODEINFO_APP:
      decodedObj["portnum"] = "NODEINFO_APP";
      break;
    default:
      decodedObj["portnum"] = "TEXT_MESSAGE_APP"; // Fallback para texto
      break;
  }
  
  // Adicionar a carga útil (payload)
  decodedObj["payload"] = packet.payload.data;
  
  // Serializar em JSON
  String jsonOutput;
  serializeJson(toRadioDoc, jsonOutput);
  return jsonOutput;
}

#endif // MESHTASTIC_PROTOBUF_H
# ESP32 Weather Station with Meshtastic Integration

Este projeto implementa uma estação meteorológica com eficiência energética usando um microcontrolador ESP32. Ele lê dados de temperatura e umidade de um sensor DHT22 e conta as basculadas do pluviômetro, depois envia esses dados para um nó Meshtastic via WiFi para propagação pela rede LoRa.

## Características

- Eficiência energética maximizada através de:
  - Limite de tempo de execução configurável com deep sleep automático
  - Duração do deep sleep personalizável (padrão: 5 minutos entre leituras)
  - Frequência da CPU definida para 160MHz para eficiência energética
- Múltiplas fontes de wake-up:
  - Baseado em timer (leituras programadas regulares)
  - Baseado em interrupção (detecção imediata de chuva)
- Integração com sensor DHT22 para leituras de temperatura e umidade
- Monitoramento de pluviômetro (0,25mm por basculada/interrupção)
- Conectividade WiFi para nó Meshtastic
- Transmissão de dados em formato JSON para propagação na rede LoRa

## Requisitos de Hardware

- Placa de desenvolvimento ESP32
- Sensor de temperatura e umidade DHT22
- Pluviômetro com mecanismo de báscula
- Fonte de energia (bateria recomendada)

## Conexões

1. **Sensor DHT22:**
   - Conecte VCC ao 3.3V
   - Conecte GND ao GND
   - Conecte DATA ao GPIO4 (ou altere DHT_PIN em config.h)

2. **Pluviômetro:**
   - Conecte um fio ao GND
   - Conecte o outro fio ao GPIO27 (ou altere RAIN_GAUGE_INTERRUPT_PIN em config.h)
   - Use um resistor pull-up externo (10kΩ) entre GPIO27 e 3.3V

## Configuração

Edite o arquivo `include/config.h` para personalizar:

- Tempo máximo de execução antes do sleep forçado (MAX_RUNTIME_MS)
- Duração do deep sleep (DEEP_SLEEP_TIME_MINUTES)
- Configuração da frequência da CPU (CPU_FREQ_MHZ)
- Atribuições de pinos para sensores
- Credenciais WiFi e timeout
- Endereço IP, porta e endpoint da API do nó Meshtastic
- Calibração do pluviômetro (RAIN_MM_PER_TIP)

## Instalação com PlatformIO

1. Certifique-se de ter o Visual Studio Code e a extensão PlatformIO instalados.
2. Clone este repositório ou faça o download dos arquivos.
3. Abra a pasta do projeto no VSCode.
4. O PlatformIO detectará automaticamente o projeto.
5. Edite as configurações em `include/config.h`.
6. Clique em "Build" e depois em "Upload" na barra inferior do VSCode.

## Considerações sobre Consumo de Energia

- O ESP32 entra em deep sleep entre leituras para conservar energia
- O WiFi só é ativado quando os dados precisam ser transmitidos
- A frequência da CPU é definida para 160MHz
- Para máxima duração da bateria, considere:
  - Aumentar o intervalo de deep sleep
  - Adicionar um interruptor de energia para desconectar completamente o circuito quando não estiver em uso
  - Usar um circuito monitor de bateria de baixa potência

## Solução de Problemas

- Se as leituras do DHT22 falharem, verifique as conexões do sensor
- Se a conexão WiFi falhar, verifique suas credenciais e a força do sinal
- Se os dados não estiverem sendo recebidos pelo nó Meshtastic:
  - Verifique o endereço IP do nó Meshtastic
  - Verifique se o endpoint da API está correto
  - Certifique-se de que o nó Meshtastic esteja configurado corretamente para aceitar solicitações HTTP

## Licença

Este projeto é lançado sob a Licença MIT.
# ESP32 Weather Station with Meshtastic and MQTT Integration

Este projeto implementa uma estação meteorológica com eficiência energética usando um microcontrolador ESP32. Ele suporta múltiplos sensores de temperatura/umidade/pressão (DHT22, AHT20, BMP280) e conta as basculadas do pluviômetro, depois envia esses dados para um nó Meshtastic via WiFi para propagação pela rede LoRa ou para um servidor MQTT para integração com sistemas de automação residencial ou IoT.

## Características

- Eficiência energética maximizada através de:
  - Limite de tempo de execução configurável com deep sleep automático
  - Duração do deep sleep personalizável (padrão: 5 minutos entre leituras)
  - Frequência da CPU definida para 160MHz para eficiência energética
- Múltiplas fontes de wake-up:
  - Baseado em timer (leituras programadas regulares)
  - Baseado em interrupção (detecção imediata de chuva)
  - Botão de configuração (para entrar no modo de configuração)
- Suporte para diferentes sensores (configuráveis em tempo de compilação):
  - DHT22 (temperatura e umidade)
  - AHT20 (temperatura e umidade de alta precisão)
  - BMP280 (temperatura e pressão barométrica)
- Monitoramento de pluviômetro (0,25mm por basculada/interrupção)
- Histórico de chuva inteligente:
  - Cálculo de precipitação na última hora
  - Cálculo de precipitação nas últimas 24 horas
  - Armazenamento de registros em memória RTC (persistência entre ciclos de sleep)
- Monitoramento de bateria:
  - Medição de tensão através do ADC
  - Cálculo de nível de bateria em percentual
  - Dados enviados junto com as informações meteorológicas
- Sincronização de horário via NTP:
  - Obtenção de timestamp real após conexão com internet
  - Utilização de timestamp real nos registros de chuva
  - Inclusão de timestamp nos dados MQTT e Meshtastic
- Conectividade WiFi com suporte para:
  - Nó Meshtastic para propagação de dados na rede LoRa
  - Servidor MQTT para integração com sistemas de automação e IoT
- Transmissão de dados em formato JSON para fácil processamento
- Interface de configuração remota:
  - Portal web acessível via WiFi quando no modo de configuração
  - Configuração BLE para ajuste de parâmetros via smartphone
  - Persistência de configurações usando SPIFFS
  - Modo de configuração automático quando falha conexão WiFi

## Requisitos de Hardware

- Placa de desenvolvimento ESP32
- Um dos seguintes sensores (conforme ambiente selecionado no PlatformIO):
  - Sensor de temperatura e umidade DHT22
  - Sensor de temperatura e umidade AHT20 (I2C)
  - Sensor de temperatura e pressão BMP280 (I2C)
- Pluviômetro com mecanismo de báscula
- Fonte de energia (bateria recomendada)

## Conexões

1. **Para o sensor DHT22:**
   - Conecte VCC ao 3.3V
   - Conecte GND ao GND
   - Conecte DATA ao GPIO4 (ou altere DHT_PIN em config.h)

2. **Para sensores I2C (AHT20 ou BMP280):**
   - Conecte VCC ao 3.3V
   - Conecte GND ao GND
   - Conecte SDA ao GPIO21 (ou altere I2C_SDA_PIN em config.h)
   - Conecte SCL ao GPIO22 (ou altere I2C_SCL_PIN em config.h)

3. **Pluviômetro:**
   - Conecte um fio ao GND
   - Conecte o outro fio ao GPIO27 (ou altere RAIN_GAUGE_INTERRUPT_PIN em config.h)
   - Use um resistor pull-up externo (10kΩ) entre GPIO27 e 3.3V
   
4. **Monitoramento de Bateria:**
   - Conecte o positivo da bateria ao pino ADC (GPIO35 por padrão)
   - Use um divisor de tensão para baterias acima de 3.6V (por exemplo, dois resistores de 100kΩ em série)
   - O código está configurado para um divisor que reduz a tensão pela metade

## Configuração

Existem duas formas de configurar o dispositivo:

### 1. Via Portal Web

Para acessar o portal de configuração:
1. Pressione o botão BOOT do ESP32 para acordar o dispositivo em modo de configuração
2. Ou, reinicie o dispositivo enquanto mantém o botão BOOT pressionado
3. O ESP32 criará um ponto de acesso WiFi chamado `ESP32-Weather-XXXXXX`
4. Conecte-se a esta rede usando a senha `weatherconfig`
5. Navegue para http://192.168.4.1 em seu navegador
6. Configure todos os parâmetros necessários na interface web

### 2. Via Bluetooth Low Energy (BLE)

1. Ative o modo de configuração conforme descrito acima
2. Use um aplicativo BLE Scanner ou aplicativo específico para ESP32
3. Conecte-se ao dispositivo `ESP32-Weather`
4. Acesse o serviço UUID `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
5. Leia/escreva na característica UUID `beb5483e-36e1-4688-b7f5-ea07361b26a8`
6. As configurações são enviadas/recebidas em formato JSON

### 3. Edição Manual

Edite o arquivo `include/config.h` antes da compilação para personalizar:

- Tempo máximo de execução antes do sleep forçado (MAX_RUNTIME_MS)
- Duração do deep sleep (DEFAULT_DEEP_SLEEP_TIME_MINUTES)
- Configuração da frequência da CPU (DEFAULT_CPU_FREQ_MHZ)
- Atribuições de pinos para sensores
- Credenciais WiFi padrão (DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD)
- Endereço IP, porta e endpoint da API do nó Meshtastic
- Configurações do servidor MQTT (servidor, porta, credenciais, tópico, intervalo)
- Calibração do pluviômetro (DEFAULT_RAIN_MM_PER_TIP)

## Instalação com PlatformIO

1. Certifique-se de ter o Visual Studio Code e a extensão PlatformIO instalados.
2. Clone este repositório ou faça o download dos arquivos.
3. Abra a pasta do projeto no VSCode.
4. O PlatformIO detectará automaticamente o projeto.
5. Edite as configurações em `include/config.h`.
6. Selecione o ambiente correto na barra inferior do VSCode:
   - `dht22` - Para usar o sensor DHT22 com Meshtastic
   - `dht22_mqtt` - Para usar o sensor DHT22 com MQTT
   - `i2c_sensors_meshtastic` - Para usar os sensores AHT20 e BMP280 juntos com Meshtastic
   - `i2c_sensors_mqtt` - Para usar os sensores AHT20 e BMP280 juntos com MQTT
7. Clique em "Build" e depois em "Upload" na barra inferior do VSCode.

Observe que o código será compilado apenas com as partes relevantes para os sensores selecionados, reduzindo o tamanho do binário final e otimizando o uso de memória. Nos ambientes `i2c_sensors_meshtastic` e `i2c_sensors_mqtt`, o sistema utilizará o AHT20 para leituras de temperatura e umidade, e o BMP280 para leituras de pressão barométrica, fornecendo um conjunto mais completo de dados meteorológicos.

## Considerações sobre Consumo de Energia

- O ESP32 entra em deep sleep entre leituras para conservar energia
- O WiFi só é ativado quando os dados precisam ser transmitidos
- A frequência da CPU é definida para 160MHz
- Para máxima duração da bateria, considere:
  - Aumentar o intervalo de deep sleep
  - Adicionar um interruptor de energia para desconectar completamente o circuito quando não estiver em uso
  - Usar um circuito monitor de bateria de baixa potência

## Solução de Problemas

- Problemas com sensores:
  - Se as leituras do DHT22 falharem, verifique as conexões do sensor
  - Se as leituras do AHT20 ou BMP280 falharem:
    - Verifique as conexões I2C (SDA e SCL)
    - Confirme se o endereço I2C do BMP280 está correto (0x76 padrão, alguns usam 0x77)
    - Verifique se os resistores pull-up estão presentes nos pinos I2C (4.7kΩ recomendados)
    - Certifique-se de que apenas um sensor está conectado ao barramento I2C durante os testes iniciais

- Se a conexão WiFi falhar:
  - O portal de configuração será iniciado automaticamente
  - Você pode se conectar ao ponto de acesso WiFi e atualizar as credenciais
  - O dispositivo tentará conectar-se novamente após salvar as novas configurações

- Se os dados não estiverem sendo recebidos pelo nó Meshtastic:
  - Verifique o endereço IP do nó Meshtastic usando o portal de configuração
  - Certifique-se que o ESP32 e o nó Meshtastic estão na mesma rede WiFi
  - Verifique se o nó Meshtastic está acessível (o código testa com conexão TCP)
  - Confirme que o nó Meshtastic tem a API HTTP habilitada (nas configurações do Meshtastic)
  - O sistema verifica a conectividade com o nó antes de tentar enviar mensagens
  - Os erros de conexão são registrados no console serial para diagnóstico
  - Códigos de erro comuns:
    - CONNECT_FAIL (-11): não conseguiu conectar ao host (IP incorreto ou host inacessível)
    - CONNECTION REFUSED (-1): o servidor está acessível mas recusou a conexão
    - NOT CONNECTED (-4): a conexão WiFi não está estabelecida
  - O sistema usa a API `/api/v1/toradio` com método PUT para enviar mensagens
  - Implementação de Protocol Buffers (nanopb) para construir mensagens compatíveis
  - As mensagens são estruturadas nativamente seguindo o formato Meshtastic:
    - MeshPacket: estrutura nativa do tipo pacote mesh contendo:
      - from: 0 (usa o ID do próprio nó)
      - to: BROADCAST_ADDR (0xffffffff para broadcast)
      - id: um número aleatório para identificar a mensagem
      - want_ack: definido como false por não necessitar confirmação
      - port: TEXT_MESSAGE_APP (porta 1) para mensagens de texto
      - payload: estrutura DataPacket contendo os dados meteorológicos em JSON
  - A biblioteca processa automaticamente a conversão entre estruturas nativas e JSON
  - A estrutura interna garante compatibilidade com o formato esperado pelo Meshtastic
  - Verifique os logs do dispositivo Meshtastic para confirmar a recepção da mensagem
  - Se necessário, tente reiniciar o nó Meshtastic para garantir que a API HTTP esteja funcionando corretamente
  
- Problemas com o modo de configuração:
  - Se o portal web não iniciar, pressione o botão RESET seguido do botão BOOT
  - Se o dispositivo BLE não aparecer na lista de dispositivos, verifique se o BLE está ativado no seu smartphone
  - Se as configurações não persistirem após reiniciar, pode haver um problema com o sistema de arquivos

- Problemas com MQTT:
  - Verifique se o servidor MQTT está acessível na rede (o código verifica a conectividade)
  - Certifique-se que as credenciais MQTT (usuário/senha) estão corretas
  - Verifique se o tópico MQTT tem permissões adequadas para publicação
  - Se o MQTT falhar, o sistema tentará usar o Meshtastic como fallback (se configurado)
  - Os códigos de erro MQTT são exibidos no console serial para diagnóstico
  - Se o intervalo de atualização MQTT estiver muito alto, considere a duração da bateria

- Para os ambientes com sensores I2C:
  - Se somente um dos sensores for encontrado durante a inicialização, o código ainda funcionará com funcionalidade limitada
  - Se preferir usar apenas um sensor I2C, você pode editar o arquivo platformio.ini para remover uma das flags de compilação

- Histórico de chuva:
  - O sistema armazena registros na memória RTC que persiste durante o deep sleep
  - Caso ocorra uma reinicialização completa ou perda de energia, o histórico será reiniciado
  - Os valores de precipitação na última hora e últimas 24 horas são calculados em tempo real
  - Se a hora do sistema for reiniciada (overflow do millis()), os cálculos serão ajustados automaticamente
  - Os registros muito antigos (>24h) são removidos periodicamente para economizar memória
  - Os dados de chuva são transmitidos com as seguintes chaves no JSON:
    - "rain": precipitação total desde o último reset (mm)
    - "rain_1h": precipitação na última hora (mm)
    - "rain_24h": precipitação nas últimas 24 horas (mm)

- Monitoramento de bateria:
  - A tensão é medida através do pino ADC GPIO35 (configurável em BATTERY_ADC_PIN)
  - O sistema espera um divisor de tensão que reduz a tensão pela metade (para baterias LiPo de 3.7-4.2V)
  - Certifique-se que o divisor não exceda a tensão máxima do ADC (3.6V)
  - Os dados de bateria são transmitidos com as seguintes chaves no JSON:
    - "voltage": tensão da bateria em volts
    - "BatteryLevel": nível estimado da bateria em percentual (100%, 75%, 50%, 25%, 10%)

- Sincronização NTP:
  - O sistema tentará sincronizar o relógio com servidores NTP após a conexão WiFi bem-sucedida
  - Se a sincronização falhar, o sistema continuará usando timestamps relativos (baseados em millis())
  - A sincronização é tentada uma vez por ciclo de execução ou se a última sincronização ocorreu há mais de 1 hora
  - Os timestamps NTP são usados para calcular os intervalos de chuva de forma mais precisa
  - Os dados enviados por MQTT incluem os seguintes campos relacionados a tempo:
    - "uptime": tempo de execução do ESP32 em segundos desde o boot
    - "timestamp": timestamp Unix (segundos desde 1970-01-01)
    - "iso_time": timestamp formatado em ISO 8601 (YYYY-MM-DDThh:mm:ssZ)

## Licença

Este projeto é lançado sob a Licença MIT.
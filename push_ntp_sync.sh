#!/bin/bash

# Script para enviar as alterações da sincronização NTP para o GitHub

# Configuração
EMAIL="${GITHUB_EMAIL}"
USERNAME="${GITHUB_USERNAME}"
TOKEN="${GITHUB_TOKEN}"
REPO="esp32-weather-station"

# Verificar se as variáveis de ambiente estão definidas
if [ -z "$EMAIL" ] || [ -z "$USERNAME" ] || [ -z "$TOKEN" ]; then
  echo "Erro: Variáveis de ambiente GITHUB_EMAIL, GITHUB_USERNAME ou GITHUB_TOKEN não definidas."
  exit 1
fi

# Configurar git
git config --global user.email "$EMAIL"
git config --global user.name "$USERNAME"

# Adicionar as alterações relacionadas à sincronização NTP
git add include/config.h
git add src/main.cpp
git add README.md
git add homeassistant_sensor_config.yaml

# Commit das alterações
git commit -m "Implementa sincronização de horário via NTP e timestamp em dados de chuva e MQTT

- Adiciona sincronização com servidores NTP após conexão WiFi
- Utiliza timestamp real em registros de chuva e estatísticas
- Inclui timestamp Unix e formato ISO 8601 em mensagens MQTT e Meshtastic
- Atualiza histórico de chuva com timestamps reais
- Adiciona documentação sobre NTP no README e na configuração do Home Assistant"

# Enviar para o GitHub
git push https://${USERNAME}:${TOKEN}@github.com/${USERNAME}/${REPO}.git

echo "Alterações da sincronização NTP enviadas para o GitHub com sucesso!"
#!/bin/bash

# Script para enviar as alterações do monitoramento de bateria para o GitHub

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

# Adicionar as alterações relacionadas ao monitoramento de bateria
git add include/config.h
git add src/main.cpp
git add README.md
git add homeassistant_sensor_config.yaml

# Commit das alterações
git commit -m "Adicionado sistema de monitoramento de bateria com tensão e nível percentual"

# Enviar para o GitHub
git push https://${USERNAME}:${TOKEN}@github.com/${USERNAME}/${REPO}.git

echo "Alterações do monitoramento de bateria enviadas para o GitHub com sucesso!"
#!/bin/bash

# Ensure the repository is initialized with the correct credentials
git config user.name "${GITHUB_USERNAME}"
git config user.email "${GITHUB_EMAIL}"

# Add changes
git add src/main.cpp homeassistant_sensor_config.yaml

# Create a commit
git commit -m "Correção no timestamp NTP para os registros de chuva

- Removidos campos desnecessários (uptime e iso_time) das mensagens MQTT e Meshtastic
- Corrigida lógica do histórico de chuva para gerenciar registros em todas as execuções
- Atualizado o cálculo de chuva após a sincronização NTP
- Atualizado YAML do Home Assistant para remover sensores não utilizados"

# Push to GitHub
git push https://${GITHUB_USERNAME}:${GITHUB_TOKEN}@github.com/${GITHUB_USERNAME}/esp32-weather-station.git main

echo "Changes pushed to GitHub successfully!"
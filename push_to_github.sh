#!/bin/bash

# Configurar Git com as credenciais do usuário
git config --global user.name "$GITHUB_USERNAME"
git config --global user.email "$GITHUB_EMAIL"

# Mostrar configurações
echo "Git configurado para:"
git config --global user.name
git config --global user.email

# Criar um diretório temporário e clonar o repositório
TEMP_DIR=$(mktemp -d)
cd $TEMP_DIR

# Clonar o repositório usando o token
echo "Clonando o repositório..."
git clone https://$GITHUB_USERNAME:$GITHUB_TOKEN@github.com/$GITHUB_USERNAME/ESP32-Pluviometer-Temp-Humidity-Meshtastic.git
cd ESP32-Pluviometer-Temp-Humidity-Meshtastic

# Copiar arquivos atualizados do projeto atual
echo "Copiando arquivos atualizados..."
cp -r /home/runner/workspace/* .

# Verificar diferenças
git status

# Adicionar todas as mudanças
git add .

# Fazer commit das mudanças
git commit -m "Melhoria no registro de timestamps para dados de chuva

- Removidos campos uptime e iso_time das mensagens MQTT e Meshtastic
- Corrigida lógica do histórico de chuva para gerenciar registros em todas as execuções
- Adicionado recálculo de valores de chuva após sincronização NTP
- Atualizado YAML do Home Assistant para remover sensores não utilizados"

# Enviar as mudanças para o GitHub
echo "Enviando mudanças para o GitHub..."
git push

echo "Processo finalizado."
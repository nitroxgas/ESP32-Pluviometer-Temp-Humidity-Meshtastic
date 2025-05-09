#!/bin/bash

# Script para enviar as modificações do sistema de histórico de chuva para o GitHub

echo "Enviando alterações do sistema de histórico de chuva para o GitHub..."

# Verificar se temos credenciais configuradas
if [ -z "$GITHUB_TOKEN" ] || [ -z "$GITHUB_USERNAME" ] || [ -z "$GITHUB_EMAIL" ]; then
  echo "Erro: Variáveis de ambiente GITHUB_TOKEN, GITHUB_USERNAME ou GITHUB_EMAIL não configuradas."
  exit 1
fi

# Configurar git
git config --global user.name "$GITHUB_USERNAME"
git config --global user.email "$GITHUB_EMAIL"

# Configurar credenciais - usando token de acesso pessoal
git config --global credential.helper store
echo "https://$GITHUB_USERNAME:$GITHUB_TOKEN@github.com" > ~/.git-credentials

# Adicionar arquivos alterados
git add src/main.cpp include/config.h README.md

# Fazer commit
git commit -m "Implementa sistema de histórico de chuva com cálculos para 1h e 24h

- Adicionada estrutura RainRecord para armazenar histórico de precipitação
- Implementadas funções para calcular chuva na última hora e últimas 24 horas
- Adicionadas variáveis RTC para persistência entre ciclos de deep sleep
- Atualizado sistema de transmissão MQTT e Meshtastic para incluir novos dados
- Documentação atualizada com informações sobre o novo sistema"

# Enviar para o GitHub
git push origin master

echo "Alterações enviadas com sucesso!"
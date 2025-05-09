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
git commit -m "Implementa sistema de histórico de chuva com cálculos para 1h e 24h

- Adicionada estrutura RainRecord para armazenar histórico de precipitação
- Implementadas funções para calcular chuva na última hora e últimas 24 horas
- Adicionadas variáveis RTC para persistência entre ciclos de deep sleep
- Atualizado sistema de transmissão MQTT e Meshtastic para incluir novos dados
- Documentação atualizada com informações sobre o novo sistema"

# Enviar as mudanças para o GitHub
echo "Enviando mudanças para o GitHub..."
git push

echo "Processo finalizado."
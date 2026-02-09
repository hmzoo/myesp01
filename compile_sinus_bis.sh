#!/bin/bash

# Script de compilation pour esp01_sinus_bis
# Compile le projet avec Arduino CLI et affiche la taille du firmware

echo "=== Compilation de esp01_sinus_bis ==="
echo ""

# Répertoire du projet
PROJECT_DIR="/home/hmj/Documents/projets/myesp01"
SKETCH="$PROJECT_DIR/src/esp01_sinus_bis/esp01_sinus_bis.ino"
BUILD_DIR="$PROJECT_DIR/build/esp01_sinus_bis"

# Vérifier que le sketch existe
if [ ! -f "$SKETCH" ]; then
    echo "❌ Erreur: Le fichier $SKETCH n'existe pas"
    exit 1
fi

# Configuration du board ESP-01 (ESP8266)
FQBN="esp8266:esp8266:generic"

# Options de compilation pour ESP-01
# - Flash size: 1MB (pas de SPIFFS)
# - CPU Frequency: 80MHz (économie d'énergie)
BUILD_OPTIONS="eesz=1M,xtal=80,baud=115200"

echo "📦 Nettoyage du répertoire de build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo ""
echo "🔨 Compilation en cours..."
echo "   Board: $FQBN"
echo "   Options: $BUILD_OPTIONS"
echo "   Sketch: $SKETCH"
echo ""

# Compilation
arduino-cli compile \
    --fqbn "$FQBN:$BUILD_OPTIONS" \
    --build-path "$BUILD_DIR" \
    "$SKETCH"

# Vérifier le résultat
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Compilation réussie!"
    echo ""
    echo "📊 Taille du firmware:"
    arduino-cli compile \
        --fqbn "$FQBN:$BUILD_OPTIONS" \
        --build-path "$BUILD_DIR" \
        "$SKETCH" 2>&1 | grep -E "Sketch uses|Global variables"
    
    echo ""
    echo "📂 Fichier binaire:"
    ls -lh "$BUILD_DIR"/esp01_sinus_bis.ino.bin 2>/dev/null || ls -lh "$BUILD_DIR"/*.bin 2>/dev/null
    
    echo ""
    echo "💾 Pour flasher sur l'ESP-01:"
    echo "   esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash 0x00000 $BUILD_DIR/esp01_sinus_bis.ino.bin"
else
    echo ""
    echo "❌ Erreur de compilation"
    exit 1
fi

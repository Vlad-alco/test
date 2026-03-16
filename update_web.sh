#!/bin/bash
# ============================================================
# Скрипт для обновления Web интерфейса в PROGMEM
# 
# Использование:
#   1. Отредактируй index.html
#   2. Запусти: ./update_web.sh
#   3. Компилируй и загружай прошивку
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INDEX_HTML="$SCRIPT_DIR/index.html"
OUTPUT_H="$SCRIPT_DIR/index_html_gz.h"

echo "=========================================="
echo "  Web Interface PROGMEM Updater"
echo "=========================================="

# Проверяем наличие index.html
if [ ! -f "$INDEX_HTML" ]; then
    echo "ERROR: index.html not found!"
    echo "Expected: $INDEX_HTML"
    exit 1
fi

# Получаем размер оригинала
ORIG_SIZE=$(wc -c < "$INDEX_HTML")
echo ""
echo "Original index.html: $ORIG_SIZE bytes ($(($ORIG_SIZE / 1024)) KB)"

# Создаём временный файл для gzip
TEMP_GZ=$(mktemp)
gzip -c -9 "$INDEX_HTML" > "$TEMP_GZ"
GZ_SIZE=$(wc -c < "$TEMP_GZ")
echo "Compressed (gzip):  $GZ_SIZE bytes ($(($GZ_SIZE / 1024)) KB)"
echo "Compression ratio:  $(echo "scale=1; $GZ_SIZE * 100 / $ORIG_SIZE" | bc)%"

# Генерируем C header файл
echo ""
echo "Generating index_html_gz.h..."

cat > "$OUTPUT_H" << 'HEADER'
// AUTO-GENERATED: Gzipped index.html for PROGMEM
// DO NOT EDIT MANUALLY! Edit index.html and run update_web.sh
//
// Generated: TIMESTAMP
// Original:  ORIG_SIZE bytes
// Compressed: GZ_SIZE bytes

#ifndef INDEX_HTML_GZ_H
#define INDEX_HTML_GZ_H

#include <Arduino.h>

const unsigned int INDEX_HTML_GZ_LEN = GZ_SIZE;

const uint8_t INDEX_HTML_GZ[] PROGMEM = {
HEADER

# Добавляем timestamp
sed -i "s/TIMESTAMP/$(date '+%Y-%m-%d %H:%M:%S')/" "$OUTPUT_H"
sed -i "s/ORIG_SIZE/$ORIG_SIZE/" "$OUTPUT_H"
sed -i "s/GZ_SIZE/$GZ_SIZE/" "$OUTPUT_H"

# Конвертируем binary в hex массив
echo "Converting to C array..."
python3 -c "
import sys
with open('$TEMP_GZ', 'rb') as f:
    data = f.read()

with open('$OUTPUT_H', 'a') as f:
    for i in range(len(data)):
        if i % 16 == 0:
            f.write('  ')
        f.write(f'0x{data[i]:02x}')
        if i < len(data) - 1:
            f.write(', ')
        if i % 16 == 15:
            f.write('\n')
    f.write('\n};\n\n#endif // INDEX_HTML_GZ_H\n')
"

# Удаляем временный файл
rm -f "$TEMP_GZ"

echo ""
echo "=========================================="
echo "  SUCCESS! index_html_gz.h updated"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Verify changes in index_html_gz.h"
echo "  2. Compile & upload firmware to ESP32"
echo ""

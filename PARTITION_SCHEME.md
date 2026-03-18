# Partition Scheme — Настройка памяти ESP32-S3

## Общая информация

ESP32-S3 имеет 8MB flash памяти, которую необходимо разделить между:
- **Application** — прошивка (скетч)
- **SPIFFS/LittleFS** — файловая система для Web-файлов
- **NVS** — Non-Volatile Storage (настройки WiFi и т.д.)
- **OTA** — область для обновления по воздуху (опционально)

---

## 1. Arduino IDE: Partition Scheme

### Выбор схемы разделов

Для работы с LittleFS (Web-файлы во внутренней flash) необходимо выбрать подходящую схему:

```
Tools → Partition Scheme → "8M with 3MB SPIFFS"
```

Или аналогичную с LittleFS:
- "8M with 2MB SPIFFS"
- "8M with 3MB SPIFFS"  ← **рекомендуется**
- "8M with 1MB SPIFFS" (если прошивка большая)

### Расположение настройки

```
Arduino IDE
├── File
├── Edit
├── Tools
│   ├── Board: "ESP32S3 Dev Module"
│   ├── USB Mode: "Hardware CDC and JTAG"
│   ├── Partition Scheme: "8M with 3MB SPIFFS" ← ЗДЕСЬ
│   └── ...
```

### Размеры разделов (для "8M with 3MB SPIFFS")

| Раздел | Размер | Назначение |
|--------|--------|------------|
| nvs | 16 KB | Настройки WiFi, Preferences |
| otadata | 8 KB | Данные OTA |
| app0 | ~3 MB | Основное приложение |
| app1 | ~3 MB | OTA резерв (если используется) |
| spiffs | 3 MB | LittleFS — Web-файлы |

---

## 2. Почему важен Partition Scheme

### Проблема (Сессия 20)

Web-интерфейс зависал при аварии TSA из-за конфликта SPI шины:
- Core 0 (Network Task) читал Web-файлы с SD карты
- Core 1 (loop) писал логи на SD карту
- SPI шина занята → Web блокируется

### Решение

Web-файлы перенесены в LittleFS (внутренняя flash ESP32):
- LittleFS НЕ использует SPI шину
- SD карта используется только для логов
- Нет конфликта между ядрами

### Требование

Для хранения Web-файлов в LittleFS нужно выделить минимум **2-3 MB** под файловую систему.

---

## 3. Проверка доступного места

### В мониторе порта при старте

```
[System] LittleFS mounted: 3 MB total, 2.8 MB free
```

### Программно

```cpp
#include <LittleFS.h>

void checkLittleFS() {
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
        Serial.printf("Total: %lu bytes\n", fs_info.totalBytes);
        Serial.printf("Used: %lu bytes\n", fs_info.usedBytes);
        Serial.printf("Free: %lu bytes\n", fs_info.totalBytes - fs_info.usedBytes);
    }
}
```

---

## 4. WebSync — синхронизация SD → LittleFS

### Структура файлов

```
SD карта (/www/):
├── index.html      → копируется в LittleFS
├── help.html       → копируется в LittleFS
└── manifest.txt    → даты файлов для синхронизации

LittleFS (корень):
├── index.html
├── help.html
└── manifest.txt
```

### manifest.txt — формат

```
# Web files version manifest
# Format: filename=YYYYMMDDHHMM
index.html=202503181430
help.html=202503151200
```

### Обновление Web-интерфейса

1. Скопировать новые файлы на SD в папку `/www/`
2. Обновить даты в `manifest.txt`
3. Перезагрузить ESP32
4. Система автоматически скопирует обновлённые файлы в LittleFS

---

## 5. Распределение памяти ESP32-S3

```
┌─────────────────────────────────────────────────────────────┐
│                    8 MB Flash Memory                        │
├─────────────────────────────────────────────────────────────┤
│  Application (прошивка)    │  ~3 MB (Partition Scheme)      │
├────────────────────────────┼────────────────────────────────┤
│  SPIFFS/LittleFS           │  3 MB (Web-файлы, настройки)   │
├────────────────────────────┼────────────────────────────────┤
│  NVS + OTA data            │  ~24 KB (WiFi, preferences)    │
├────────────────────────────┼────────────────────────────────┤
│  Bootloader                │  ~24 KB                         │
└────────────────────────────┴────────────────────────────────┘
```

### RAM (512 KB SRAM + 8 MB PSRAM)

| Тип | Размер | Назначение |
|-----|--------|------------|
| SRAM | 512 KB | Переменные, стек, куча |
| PSRAM | 8 MB | Большие буферы (опционально) |

---

## 6. Возможные ошибки

### "LittleFS mount failed"

**Причина**: Partition Scheme не выделает место под SPIFFS

**Решение**: Выбрать "8M with 3MB SPIFFS" и перезалить прошивку

### "Not enough space"

**Причина**: Скетч слишком большой для выбранной схемы

**Решение**:
- Выбрать схему с меньшим SPIFFS (1-2 MB)
- Оптимизировать код
- Удалить неиспользуемые библиотеки

### "SPIFFS size mismatch"

**Причина**: Размер раздела не совпадает с форматированием

**Решение**:
```cpp
// Полное переформатирование LittleFS
LittleFS.format();
LittleFS.begin();
```

---

## 7. Таблица совместимости

| Partition Scheme | App размер | SPIFFS | Рекомендация |
|------------------|------------|--------|--------------|
| "Default 4MB" | 1.2 MB | 1.5 MB | Не подходит (8MB flash) |
| "8M with 1MB SPIFFS" | ~5 MB | 1 MB | Мало для Web + логи |
| "8M with 2MB SPIFFS" | ~4 MB | 2 MB | Минимум для WebSync |
| "8M with 3MB SPIFFS" | ~3 MB | 3 MB | **Рекомендуется** |

---

## 8. История изменений памяти

### Сессия 20 (2025-03-18)

**Проблема**: Web зависал при аварии TSA

**Решение**: Web-файлы перенесены в LittleFS

**Требование**: Partition Scheme с минимум 2MB SPIFFS

**Выбрано**: "8M with 3MB SPIFFS"

---

## Ссылки

- [ESP32 Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)
- [LittleFS for ESP32](https://github.com/earlephilhower/arduino-esp32littlefs-plugin)
- [CHANGELOG.md](CHANGELOG.md) — история изменений проекта

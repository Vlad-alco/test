#ifndef SDLOGGER_H
#define SDLOGGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <time.h> 
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// === ИЗМЕНЕНО: Увеличен лимит с 50 КБ до 200 КБ ===
// 200 КБ хватит на 2-3 часа работы с детальным логом
const unsigned long MAX_LOG_SIZE = 204800; 
// ==================================================

// === ГЛОБАЛЬНЫЙ МЬЮТЕКС ДЛЯ SD КАРТЫ ===
// Объявлен в .ino файле, создаётся в setup()
// Защищает SPI шину от одновременного доступа с разных ядер ESP32
extern SemaphoreHandle_t sdMutex;
// =======================================

// === КЛАСС ДЛЯ АВТОМАТИЧЕСКОГО ЗАХВАТА МЬЮТЕКСА ===
// При создании - захватывает мьютекс
// При уничтожении (выход из области видимости) - освобождает
class SDScopeLock {
public:
    bool locked = false;  // Удалось ли захватить мьютекс
    
    SDScopeLock() {
        if (sdMutex) {
            // Таймаут 100мс - если SD занята, не блокируем систему
            locked = (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE);
        } else {
            locked = true;  // Если мьютекса нет, считаем что "захватили"
        }
    }
    ~SDScopeLock() {
        if (sdMutex && locked) {
            xSemaphoreGive(sdMutex);
        }
    }
    // Запрещаем копирование
    SDScopeLock(const SDScopeLock&) = delete;
    SDScopeLock& operator=(const SDScopeLock&) = delete;
};
// ==================================================

class SDLogger {
public:
    void init() {
        // Ничего не делаем. SD должна быть инициализирована раньше (в AppNetwork).
    }

    void log(const String &message) {
        // Формируем строку (быстро, мьютекс не нужен)
        String timeStr = getTimeStr();
        String logLine = "[" + timeStr + "] " + message;

        // === КРИТИЧЕСКАЯ СЕКЦИЯ: ЛЮБАЯ работа с SD только под sdMutex ===
        // Важно: logger.log() может вызываться с разных ядер (Core 0/1),
        // поэтому проверка SD, ротация и запись должны быть синхронизированы.
        bool wroteToSd = false;
        {
            SDScopeLock lock;  // Захват мьютекса (автоматически, с таймаутом!)
            
            // Проверяем удалось ли захватить мьютекс
            if (!lock.locked) {
                // SD занята - пропускаем запись, не блокируем систему
                Serial.println("[SD BUSY] " + message);
                Serial.println(logLine);
                return;
            }

            // Одноразовая проверка доступности SD (под мьютексом!)
            if (!sdChecked) {
                sdAvailable = (SD.cardSize() > 0);
                sdChecked = true;
            }

            if (sdAvailable) {
                // Ротация логов тоже под мьютексом.
                if (currentFileSize > MAX_LOG_SIZE) {
                    rotateLogsLocked();
                    currentFileSize = 0;
                }

                File file = SD.open("/system.log", FILE_APPEND);
                if (file) {
                    file.println(logLine);
                    currentFileSize += logLine.length() + 2; // + \r\n
                    file.close();
                    wroteToSd = true;
                }
            }
        }
        // ============================================================

        if (!wroteToSd) {
            Serial.println("[NO SD] " + message);
        }

        // Выводим в Serial (не требует мьютекса)
        Serial.println(logLine);
    }

    void log(const String &label, int value) { log(label + String(value)); }
    void log(const String &label, float value) { log(label + String(value, 2)); }

    String readLastLog() {
        // === КРИТИЧЕСКАЯ СЕКЦИЯ: чтение с SD ===
        // readLastLog() может вызываться из Web (Core 0), поэтому синхронизация обязательна.
        SDScopeLock lock;  // Захват мьютекса (с таймаутом!)
        
        // Проверяем удалось ли захватить мьютекс
        if (!lock.locked) {
            return "[SD BUSY] Try again later";
        }

        if (!sdChecked) {
            sdAvailable = (SD.cardSize() > 0);
            sdChecked = true;
        }
        if (!sdAvailable) return "SD Error";

        File file = SD.open("/system.log");
        if (!file) return "Log file not found";
        
        String content = "";
        
        // === ИЗМЕНЕНО: Буфер увеличен с 4096 до 16384 байт (16 КБ) ===
        // ESP32-S3 имеет достаточно памяти для этого.
        const int bufferSize = 16384; 
        
        if (file.size() > bufferSize) {
            file.seek(file.size() - bufferSize);
        }
        
        // Резервируем память для строки, чтобы не было фрагментации кучи
        content.reserve(bufferSize); 
        
        while (file.available()) {
            content += (char)file.read();
        }
        file.close();
        // При выходе lock уничтожается -> мьютекс освобождается
        
        return content;
    }

    bool isSdAvailable() { return sdAvailable; }

private:
    bool sdAvailable = false;
    bool sdChecked = false;  // Флаг: уже проверяли SD
    // === НОВАЯ ПЕРЕМЕННАЯ ===
    unsigned long currentFileSize = 0; 
    // ========================

    String getTimeStr() {
        time_t now = time(nullptr);
        if (now > 1609459200) {
            struct tm* timeinfo = localtime(&now);
            char buffer[20];
            strftime(buffer, 20, "%d.%m.%Y %H:%M:%S", timeinfo);
            return String(buffer);
        } else {
            return String(millis() / 1000) + "s";
        }
    }

    void rotateLogsLocked() {
        // ВАЖНО: вызывать ТОЛЬКО когда sdMutex уже удерживается.
        
        Serial.println("[Logger] Rotating logs...");
        // Удаляем самый старый
        if (SD.exists("/system5.log")) SD.remove("/system5.log");
        if (SD.exists("/system4.log")) SD.rename("/system4.log", "/system5.log");
        if (SD.exists("/system3.log")) SD.rename("/system3.log", "/system4.log");
        if (SD.exists("/system2.log")) SD.rename("/system2.log", "/system3.log");
        if (SD.exists("/system1.log")) SD.rename("/system1.log", "/system2.log");
        
        // Переименовываем текущий лог
        SD.rename("/system.log", "/system1.log");
        
        // sdAvailable остается true, счетчик currentFileSize сбрасывается в log()
    }
};

extern SDLogger logger;

#endif

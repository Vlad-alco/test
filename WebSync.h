#ifndef WEBSYNC_H
#define WEBSYNC_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>

/**
 * WebSync - синхронизация Web-файлов между SD и LittleFS
 * 
 * Решает проблему конфликта SPI шины:
 * - SD карта используется для логов (Core 1)
 * - LittleFS (внутренняя flash) для Web-файлов (Core 0)
 * - LittleFS не использует SPI, поэтому нет конфликта
 * 
 * Принцип работы:
 * 1. При старте проверяет manifest.txt на SD и в LittleFS
 * 2. Сравнивает версии файлов по дате (YYYYMMDDHHMM)
 * 3. Если на SD новее — копирует в LittleFS
 * 4. WebServer отдаёт файлы из LittleFS (без обращения к SD)
 */

class WebSync {
public:
    /**
     * Инициализация LittleFS и синхронизация файлов с SD
     * Вызывается один раз при старте, ДО запуска WebServer
     * @return true если LittleFS готова к работе
     */
    static bool begin();
    
    /**
     * Проверить наличие файла в LittleFS
     * @param filename имя файла (без пути)
     * @return true если файл существует
     */
    static bool hasFile(const char* filename);
    
    /**
     * Открыть файл из LittleFS для чтения
     * @param filename имя файла (без пути)
     * @return File объект (проверить через bool оператор)
     */
    static File openFile(const char* filename);
    
private:
    // Список файлов для синхронизации
    static const char* WEB_FILES[];
    static const int WEB_FILES_COUNT;
    
    /**
     * Основная логика синхронизации
     */
    static void syncFiles();
    
    /**
     * Чтение manifest с SD карты
     * @param versions массив для записи версий
     */
    static void readManifestFromSD(unsigned long versions[]);
    
    /**
     * Чтение manifest из LittleFS
     * @param versions массив для записи версий
     */
    static void readManifestFromLittleFS(unsigned long versions[]);
    
    /**
     * Сохранение manifest в LittleFS
     * @param versions массив версий
     */
    static void saveManifestToLittleFS(unsigned long versions[]);
    
    /**
     * Парсинг версии из содержимого manifest
     * @param content содержимое файла
     * @param filename имя файла для поиска
     * @return версия (дата) или 0 если не найдено
     */
    static unsigned long parseManifestVersion(const String& content, const char* filename);
    
    /**
     * Создание содержимого manifest.txt
     * @param versions массив версий
     * @return содержимое файла
     */
    static String createManifestContent(unsigned long versions[]);
    
    /**
     * Копирование файла SD → LittleFS
     * @param filename имя файла (без пути)
     * @return true если успешно
     */
    static bool copyFileToLittleFS(const char* filename);
    
    /**
     * Получить текущую версию на основе даты/времени
     * @return число в формате YYYYMMDDHHMM или 1 если нет времени
     */
    static unsigned long getCurrentVersion();
};

#endif

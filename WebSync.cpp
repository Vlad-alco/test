#include "WebSync.h"

// Список Web-файлов для синхронизации
const char* WebSync::WEB_FILES[] = {
    "index.html",
    "help.html",
    "js/chart.umd.js",
    "js/lucide.js",
    "js/tailwindcss.js"
};
const int WebSync::WEB_FILES_COUNT = 5;

bool WebSync::begin() {
    Serial.println("[WebSync] ========================================");
    Serial.println("[WebSync] Initializing LittleFS...");
    
    // 1. Инициализация LittleFS
    // true = форматировать если не примонтирована (первый запуск)
    if (!LittleFS.begin(true)) {
        Serial.println("[WebSync] LittleFS mount FAILED!");
        Serial.println("[WebSync] Web will NOT be available!");
        return false;
    }
    
    Serial.println("[WebSync] LittleFS mounted OK.");
    
    // Вывод информации о месте
    Serial.printf("[WebSync] LittleFS total: %u bytes\n", LittleFS.totalBytes());
    Serial.printf("[WebSync] LittleFS used:  %u bytes\n", LittleFS.usedBytes());
    
    // 2. Синхронизация файлов с SD
    syncFiles();
    
    Serial.println("[WebSync] ========================================");
    return true;
}

void WebSync::syncFiles() {
    Serial.println("[WebSync] Checking for updates on SD...");
    
    // Массивы для версий
    unsigned long sdVersions[WEB_FILES_COUNT] = {0, 0};
    unsigned long fsVersions[WEB_FILES_COUNT] = {0, 0};
    
    // Читаем manifest с SD
    readManifestFromSD(sdVersions);
    
    // Читаем manifest из LittleFS
    readManifestFromLittleFS(fsVersions);
    
    // Проверяем каждый файл
    bool anyUpdated = false;
    
    for (int i = 0; i < WEB_FILES_COUNT; i++) {
        const char* filename = WEB_FILES[i];
        
        Serial.printf("[WebSync] %s: SD=%lu, Local=%lu", 
                      filename, sdVersions[i], fsVersions[i]);
        
        // Если на SD новее или файл отсутствует в LittleFS
        bool needUpdate = (sdVersions[i] > fsVersions[i]) || !hasFile(filename);
        
        if (needUpdate) {
            Serial.println(" -> UPDATING");
            if (copyFileToLittleFS(filename)) {
                fsVersions[i] = sdVersions[i];
                anyUpdated = true;
                Serial.printf("[WebSync] %s updated OK\n", filename);
            } else {
                Serial.printf("[WebSync] %s update FAILED!\n", filename);
            }
        } else {
            Serial.println(" -> OK");
        }
    }
    
    // Сохраняем обновлённый manifest в LittleFS
    if (anyUpdated) {
        saveManifestToLittleFS(fsVersions);
    }
    
    Serial.println("[WebSync] Sync complete.");
}

void WebSync::readManifestFromSD(unsigned long versions[]) {
    // Инициализируем нулями
    for (int i = 0; i < WEB_FILES_COUNT; i++) {
        versions[i] = 0;
    }
    
    // SD должна быть уже инициализирована (в .ino: appNetwork.initSD())
    File file = SD.open("/www/manifest.txt", "r");
    if (!file) {
        Serial.println("[WebSync] No manifest.txt on SD, using current time");
        // Если manifest нет — используем текущую дату как версию
        // Это заставит скопировать файлы при первом запуске
        unsigned long nowVersion = getCurrentVersion();
        for (int i = 0; i < WEB_FILES_COUNT; i++) {
            versions[i] = nowVersion;
        }
        return;
    }
    
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    
    // Парсим версии для каждого файла
    for (int i = 0; i < WEB_FILES_COUNT; i++) {
        versions[i] = parseManifestVersion(content, WEB_FILES[i]);
    }
    
    Serial.println("[WebSync] Manifest loaded from SD");
}

void WebSync::readManifestFromLittleFS(unsigned long versions[]) {
    // Инициализируем нулями
    for (int i = 0; i < WEB_FILES_COUNT; i++) {
        versions[i] = 0;
    }
    
    File file = LittleFS.open("/manifest.txt", "r");
    if (!file) {
        Serial.println("[WebSync] No manifest in LittleFS (first run)");
        return;
    }
    
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    
    // Парсим версии
    for (int i = 0; i < WEB_FILES_COUNT; i++) {
        versions[i] = parseManifestVersion(content, WEB_FILES[i]);
    }
    
    Serial.println("[WebSync] Manifest loaded from LittleFS");
}

void WebSync::saveManifestToLittleFS(unsigned long versions[]) {
    String content = createManifestContent(versions);
    
    File file = LittleFS.open("/manifest.txt", "w");
    if (!file) {
        Serial.println("[WebSync] ERROR: Cannot write manifest!");
        return;
    }
    
    file.print(content);
    file.close();
    Serial.println("[WebSync] Manifest saved to LittleFS");
}

unsigned long WebSync::parseManifestVersion(const String& content, const char* filename) {
    // Ищем строку вида: filename=YYYYMMDDHHMM
    String searchKey = String(filename) + "=";
    int pos = content.indexOf(searchKey);
    
    if (pos == -1) {
        return 0;  // Не найдено
    }
    
    int start = pos + searchKey.length();
    String versionStr = content.substring(start, start + 12);  // YYYYMMDDHHMM = 12 символов
    
    return versionStr.toInt();
}

String WebSync::createManifestContent(unsigned long versions[]) {
    String content = "# Web files version manifest\n";
    content += "# Format: filename=YYYYMMDDHHMM\n";
    
    for (int i = 0; i < WEB_FILES_COUNT; i++) {
        content += String(WEB_FILES[i]) + "=";
        // Форматирование с ведущими нулями: YYYYMMDDHHMM
        char buf[16];
        snprintf(buf, sizeof(buf), "%012lu", versions[i]);
        content += buf;
        content += "\n";
    }
    return content;
}

bool WebSync::copyFileToLittleFS(const char* filename) {
    // Пути
    String sdPath = "/www/" + String(filename);
    String fsPath = "/" + String(filename);
    
    // Проверяем наличие на SD
    if (!SD.exists(sdPath)) {
        Serial.printf("[WebSync] File not found on SD: %s\n", sdPath.c_str());
        return false;
    }
    
    // Если файл в подпапке (например js/file.js) — создаём папку
    int slashPos = fsPath.indexOf('/', 1);  // Ищем второй слеш
    if (slashPos > 0) {
        String dirPath = fsPath.substring(0, slashPos);  // "/js"
        if (!LittleFS.exists(dirPath)) {
            LittleFS.mkdir(dirPath);
            Serial.printf("[WebSync] Created directory: %s\n", dirPath.c_str());
        }
    }
    
    // Открываем исходный файл на SD
    File sdFile = SD.open(sdPath, "r");
    if (!sdFile) {
        Serial.printf("[WebSync] ERROR: Cannot open %s on SD\n", sdPath.c_str());
        return false;
    }
    
    // Получаем размер
    size_t fileSize = sdFile.size();
    Serial.printf("[WebSync] Copying %s (%u bytes)...\n", filename, fileSize);
    
    // Удаляем старый файл в LittleFS если есть
    if (LittleFS.exists(fsPath)) {
        LittleFS.remove(fsPath);
    }
    
    // Создаём файл в LittleFS
    File fsFile = LittleFS.open(fsPath, "w");
    if (!fsFile) {
        Serial.printf("[WebSync] ERROR: Cannot create %s in LittleFS\n", fsPath.c_str());
        sdFile.close();
        return false;
    }
    
    // Копируем буфером 4KB
    const size_t bufferSize = 4096;
    uint8_t* buffer = new uint8_t[bufferSize];
    
    if (!buffer) {
        Serial.println("[WebSync] ERROR: Cannot allocate buffer!");
        sdFile.close();
        fsFile.close();
        return false;
    }
    
    size_t totalCopied = 0;
    
    while (sdFile.available()) {
        size_t bytesToRead = sdFile.available() > bufferSize ? bufferSize : sdFile.available();
        size_t bytesRead = sdFile.read(buffer, bytesToRead);
        
        if (bytesRead > 0) {
            size_t bytesWritten = fsFile.write(buffer, bytesRead);
            if (bytesWritten != bytesRead) {
                Serial.printf("[WebSync] ERROR: Write mismatch at %u bytes\n", totalCopied);
                delete[] buffer;
                sdFile.close();
                fsFile.close();
                return false;
            }
            totalCopied += bytesWritten;
        }
    }
    
    delete[] buffer;
    sdFile.close();
    fsFile.close();
    
    Serial.printf("[WebSync] Copied %u bytes OK\n", totalCopied);
    return true;
}

unsigned long WebSync::getCurrentVersion() {
    // Возвращаем текущую дату/время как число YYYYMMDDHHMM
    time_t now = time(nullptr);
    if (now < 1609459200) {  // < 2021-01-01 (время не синхронизировано)
        return 1;
    }
    
    struct tm* timeinfo = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y%m%d%H%M", timeinfo);
    
    return String(buf).toInt();
}

bool WebSync::hasFile(const char* filename) {
    String path = "/" + String(filename);
    return LittleFS.exists(path);
}

File WebSync::openFile(const char* filename) {
    String path = "/" + String(filename);
    return LittleFS.open(path, "r");
}
